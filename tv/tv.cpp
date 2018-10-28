// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "tools/transform.h"
#include "util/config.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <array>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>

using namespace IR;
using namespace tools;
using namespace util;
using namespace std;
using llvm::cast, llvm::dyn_cast, llvm::isa, llvm::errs;

namespace {

#if 0
string_view s(llvm::StringRef str) {
  return { str.data(), str.size() };
}
#endif

smt::smt_initializer smt_init;
TransformPrintOpts print_opts;
array<unique_ptr<IntType>, 65> int_types;
unordered_map<string, Function> fns;
unordered_map<const llvm::Value*, Value*> identifiers;
Function *current_fn;

// cache Value*'s names
unordered_map<const llvm::Value*, std::string> value_names;
unsigned value_id_counter; // for %0, %1, etc..


struct alive_init {
  alive_init() {
    int_types[0] = nullptr;

    for (unsigned i = 1; i <= 64; ++i) {
      int_types[i] = make_unique<IntType>("i" + to_string(i), i);
    }
  }
} alive_init;

void reset_state() {
  identifiers.clear();
  current_fn = nullptr;
  value_names.clear();
  value_id_counter = 0;
}


string value_name(const llvm::Value &v) {
  auto &name = value_names[&v];
  if (!name.empty())
    return name;

  if (!v.getName().empty())
    return name = '%' + v.getName().str();
  return name = '%' + to_string(value_id_counter++);
}

Type* llvm_type2alive(const llvm::Type *ty) {
  switch (ty->getTypeID()) {
  case llvm::Type::VoidTyID:
    return &Type::voidTy;
  case llvm::Type::IntegerTyID: {
    auto bits = cast<llvm::IntegerType>(ty)->getBitWidth();
    return bits <= 64 ? int_types[bits].get() : nullptr;
  }
  default:
    errs() << "Unsupported type: " << *ty << '\n';
    return nullptr;
  }
}


Value* get_operand(llvm::Value *v) {
  if (isa<llvm::Instruction>(v) || isa<llvm::Argument>(v))
    return identifiers[v];

  auto ty = llvm_type2alive(v->getType());
  if (!ty)
    return nullptr;

  if (auto cnst = dyn_cast<llvm::ConstantInt>(v)) {
    auto c = make_unique<IntConst>(*ty, cnst->getZExtValue());
    auto ret = c.get();
    current_fn->addConstant(move(c));
    return ret;
  }

  if (isa<llvm::UndefValue>(v)) {
    auto val = make_unique<UndefValue>(*ty);
    auto ret = val.get();
    current_fn->addUndef(move(val));
    return ret;
  }

  return nullptr;
}


const unordered_map<unsigned, BinOp::Op> llvm_binop2alive = {
#define LLVM_BINOP(x, y) { x, y },
#include "tv/tv.h"
#undef LLVM_BINOP
};

unique_ptr<Instr> llvm_instr2alive(const llvm::Instruction &i) {
  switch (i.getOpcode()) {
#define LLVM_BINOP(x, y) case x:
#include "tv/tv.h"
#undef LLVM_BINOP
  {
    auto ty = llvm_type2alive(i.getType());
    auto op1 = get_operand(i.getOperand(0));
    auto op2 = get_operand(i.getOperand(1));
    if (!ty || !op1 || !op2)
      goto err;

    auto alive_op = llvm_binop2alive.at(i.getOpcode());

    unsigned flags = BinOp::None;
    if (isa<llvm::OverflowingBinaryOperator>(i) && i.hasNoSignedWrap())
      flags |= BinOp::NSW;
    if (isa<llvm::OverflowingBinaryOperator>(i) && i.hasNoUnsignedWrap())
      flags |= BinOp::NUW;
    if (isa<llvm::PossiblyExactOperator>(i) && i.isExact())
      flags = BinOp::Exact;

    auto op = make_unique<BinOp>(*ty, value_name(i), *op1, *op2, alive_op,
                                 (BinOp::Flags)flags);

    identifiers.emplace(&i, op.get());
    return op;
  }
  case llvm::Instruction::Ret:
  {
    auto ty = llvm_type2alive(i.getType());
    auto op = get_operand(i.getOperand(0));
    if (!ty || !op)
      goto err;
    return make_unique<Return>(*ty, *op);
  }
  default:
    break;
  }
err:
  errs() << "Unsupported instruction: " << i << '\n';
  return {};
}


optional<Function> llvm2alive(const llvm::Function &f) {
  reset_state();

  auto type = llvm_type2alive(f.getReturnType());
  if (!type)
    return {};

  Function Fn(*type, f.getName());
  current_fn = &Fn;

  for (auto &arg : f.args()) {
    auto ty = llvm_type2alive(arg.getType());
    if (!ty)
      return {};
    auto val = make_unique<Input>(*ty, value_name(arg));
    identifiers.emplace(&arg, val.get());
    Fn.addInput(move(val));
  }

  for (auto &bb : f) {
    auto &BB = Fn.getBB(value_name(bb));
    for (auto &i : bb) {
      if (auto I = llvm_instr2alive(i))
        BB.addIntr(move(I));
      else
        return {};
    }
  }

  return move(Fn);
}


struct TVPass : public llvm::FunctionPass {
  static char ID;

  bool FatalErrors = true;

  TVPass() : FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override {
    auto fn = llvm2alive(F);
    if (!fn) {
      fns.erase(F.getName());
      return false;
    }

    auto [old_fn, inserted] = fns.try_emplace(fn->getName(), move(*fn));
    if (inserted)
      return false;

    Transform t;
    t.src = move(old_fn->second);
    t.tgt = move(*fn);
    TransformVerify verifier(t, false);
    t.print(cout, print_opts);

    if (Errors errs = verifier.verify()) {
      if (FatalErrors)
        llvm::report_fatal_error("[Alive2] Transform doesn't verify!");
      else
        cout << "Transformation doesn't verify!\n\n";
    } else {
      cout << "Transformation seems to be correct!\n\n";
    }

    old_fn->second = move(t.tgt);
    return false;
  }

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }
};

char TVPass::ID = 0;
llvm::RegisterPass<TVPass> X("tv", "Translation Validator", false, false);

}
