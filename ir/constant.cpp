// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/constant.h"
#include "smt/expr.h"
#include "util/compiler.h"
#include <cassert>

using namespace smt;
using namespace std;

namespace IR {

void Constant::print(ostream &os) const {
  UNREACHABLE();
}

pair<expr, expr> Constant::toSMT_cnst() const {
  return { expr::mkVar(getName().c_str(), bits()), true };
}

StateValue Constant::toSMT(State &s) const {
  auto ret = toSMT_cnst();
  s.addUB(move(ret.second));
  return { move(ret.first), true };
}


ConstantBinOp::ConstantBinOp(Type &type, Constant &lhs, Constant &rhs, Op op)
  : Constant(type, ""), lhs(lhs), rhs(rhs), op(op) {
  const char *opname = nullptr;
  switch (op) {
  case ADD:  opname = " + "; break;
  case SUB:  opname = " - "; break;
  case SDIV: opname = " / "; break;
  case UDIV: opname = " /u "; break;
  }

  string str = '(' + this->lhs.getName();
  str += opname;
  str += rhs.getName();
  str += ')';
  this->setName(move(str));
}

static void div_ub(const expr &a, const expr &b, expr &ub, bool sign) {
  auto bits = b.bits();
  ub &= b != expr::mkUInt(0, bits);
  if (sign)
    ub &= (a != expr::IntMin(bits) || b != expr::mkInt(-1, bits));
}

pair<expr, expr> ConstantBinOp::toSMT_cnst() const {
  auto a = lhs.toSMT_cnst();
  auto b = rhs.toSMT_cnst();
  auto ub = move(a.second) && move(b.second);
  expr val;

  switch (op) {
  case ADD: val = a.first + b.first; break;
  case SUB: val = a.first - b.first; break;
  case SDIV:
    val = a.first.sdiv(b.first);
    div_ub(a.first, b.first, ub, true);
    break;
  case UDIV:
    val = a.first.udiv(b.first);
    div_ub(a.first, b.first, ub, false);
    break;
  }
  return { move(val), move(ub) };
}

expr ConstantBinOp::getTypeConstraints() const {
  return Value::getTypeConstraints() &&
         getType().enforceIntType() &&
         getType() == lhs.getType() &&
         getType() == rhs.getType();
}


ConstantFn::ConstantFn(Type &type, string_view name, vector<Value*> &&args) :
  Constant(type, ""), args(move(args)) {
  unsigned num_args;
  if (name == "log2") {
    fn = LOG2;
    num_args = 1;
  } else if (name == "width") {
    fn = WIDTH;
    num_args = 1;
  } else {
    throw ConstantFnException("Unknown function: " + string(name));
  }

  auto actual_args = this->args.size();
  if (actual_args != num_args)
    throw ConstantFnException("Expected " + to_string(num_args) +
                              " parameter for " + string(name) + ", but got " +
                              to_string(actual_args));

  string str = string(name) + '(';
  bool first = true;
  for (auto arg : this->args) {
    if (!first)
      str += ", ";
    first = false;
    str += arg->getName();
  }
  str += ')';
  this->setName(move(str));
}

pair<expr, expr> ConstantFn::toSMT_cnst() const {
  return { expr() /* TODO */, true };
}


void BoolPred::print(ostream &os) const {
  os << '(';
  lhs.print(os);
  os << ") " << (pred == AND ? "&&" : "||") << " (";
  rhs.print(os);
  os << ')';
}

expr BoolPred::toSMT() const {
  auto a = lhs.toSMT();
  auto b = rhs.toSMT();
  switch (pred) {
  case AND: return a && b;
  case OR:  return a || b;
  }
  UNREACHABLE();
}


void CmpPred::print(ostream &os) const {
  // TODO
}

expr CmpPred::toSMT() const {
  return expr() /* TODO */;
}

}
