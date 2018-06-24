// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/type.h"
#include "smt/solver.h"
#include "util/compiler.h"
#include <cassert>
#include <sstream>

using namespace smt;
using namespace std;

static constexpr unsigned var_type_bits = 3;
static constexpr unsigned var_bw_bits = 10;


namespace IR {

expr Type::var(const char *var, unsigned bits) const {
  assert(!opname.empty());
  auto str = opname + '_' + var;
  return expr::mkVar(str.c_str(), bits);
}

expr Type::typeVar() const {
  return var("type", var_type_bits);
}

expr Type::sizeVar() const {
  return var("bw", var_bw_bits);
}

expr Type::is(unsigned t) const {
  return typeVar() == expr::mkUInt(t, var_type_bits);
}

expr Type::isInt() const    { return is(SymbolicType::Int); }
expr Type::isFloat() const  { return is(SymbolicType::Float); }
expr Type::isPtr() const    { return is(SymbolicType::Ptr); }
expr Type::isArray() const  { return is(SymbolicType::Array); }
expr Type::isVector() const { return is(SymbolicType::Vector); }

void Type::setName(const string &name) {
  opname = name;
}

unsigned Type::bits() const {
  UNREACHABLE();
}

expr Type::operator==(const Type &b) const {
  if (this == &b)
    return true;

#define CMP(Ty)                                                                \
  if (auto lhs = dynamic_cast<const Ty*>(this)) {                              \
    if (auto rhs = dynamic_cast<const Ty*>(&b))                                \
      return *lhs == *rhs;                                                     \
    if (auto rhs = dynamic_cast<const SymbolicType*>(&b))                      \
      return *rhs == *lhs;                                                     \
    return false;                                                              \
  }

  CMP(IntType)
  CMP(FloatType)
  CMP(PtrType)
  CMP(ArrayType)
  CMP(VectorType)

#undef CMP

  if (auto lhs = dynamic_cast<const SymbolicType*>(this)) {
    return *lhs == b;
  }

  return false;
}

void Type::enforceIntType() {
  UNREACHABLE();
}

void Type::enforceIntOrPtrOrVectorType() {
  UNREACHABLE();
}

string Type::toString() const {
  stringstream s;
  print(s);
  return s.str();
}

Type::~Type() {}


expr VoidType::getTypeConstraints() const {
  return true;
}

void VoidType::fixup(const Model &m) {
  // do nothing
}

unique_ptr<Type> VoidType::dup() const {
  return make_unique<VoidType>();
}

void VoidType::print(ostream &os) const {
  os << "void";
}


unsigned IntType::bits() const {
  return bitwidth;
}

expr IntType::getTypeConstraints() const {
  // limit ints to be between 1 and 64 bits
  // TODO: lift 64-bit restriction
  auto bw = sizeVar();
  return bw != expr::mkUInt(0, var_bw_bits) &&
         bw.ule(expr::mkUInt(64, var_bw_bits));
}

expr IntType::sizeVar() const {
  return defined ? expr::mkUInt(bits(), var_bw_bits) : Type::sizeVar();
}

expr IntType::operator==(const IntType &rhs) const {
  return sizeVar() == rhs.sizeVar();
}

void IntType::fixup(const Model &m) {
  if (!defined)
    bitwidth = m.getUInt(sizeVar());
}

void IntType::enforceIntType() {
  // do nothing
}

void IntType::enforceIntOrPtrOrVectorType() {
  // do nothing
}

unique_ptr<Type> IntType::dup() const {
  return make_unique<IntType>(*this);
}

void IntType::print(ostream &os) const {
  if (defined)
    os << 'i' << bits();
}


expr FloatType::getTypeConstraints() const {
  // TODO
  return false;
}

expr FloatType::operator==(const FloatType &rhs) const {
  // TODO
  return false;
}

void FloatType::fixup(const Model &m) {
  // TODO
}

unique_ptr<Type> FloatType::dup() const {
  return make_unique<FloatType>(*this);
}

void FloatType::print(ostream &os) const {
  os << "TODO";
}


expr PtrType::getTypeConstraints() const {
  // TODO
  return false;
}

expr PtrType::operator==(const PtrType &rhs) const {
  // TODO
  return sizeVar() == rhs.sizeVar();
}

void PtrType::fixup(const Model &m) {
  // TODO
}

void PtrType::enforceIntOrPtrOrVectorType() {
  // do nothing
}

unique_ptr<Type> PtrType::dup() const {
  return make_unique<PtrType>(*this);
}

void PtrType::print(ostream &os) const {
  os << "TODO";
}


expr ArrayType::getTypeConstraints() const {
  // TODO
  return false;
}

expr ArrayType::operator==(const ArrayType &rhs) const {
  // TODO
  return false;
}

void ArrayType::fixup(const Model &m) {
  // TODO
}

unique_ptr<Type> ArrayType::dup() const {
  return make_unique<ArrayType>(*this);
}

void ArrayType::print(ostream &os) const {
  os << "TODO";
}


expr VectorType::getTypeConstraints() const {
  // TODO
  return false;
}

expr VectorType::operator==(const VectorType &rhs) const {
  // TODO
  return false;
}

void VectorType::fixup(const Model &m) {
  // TODO
}

void VectorType::enforceIntOrPtrOrVectorType() {
  // do nothing
}

unique_ptr<Type> VectorType::dup() const {
  return make_unique<VectorType>(*this);
}

void VectorType::print(ostream &os) const {
  os << "TODO";
}


expr SymbolicType::isInt() const {
  return expr(enabled & (1 << Int)) && Type::isInt();
}

expr SymbolicType::isFloat() const {
  return expr(enabled & (1 << Float)) && Type::isFloat();
}

expr SymbolicType::isPtr() const {
  return expr(enabled & (1 << Ptr)) && Type::isPtr();
}

expr SymbolicType::isVector() const {
  return expr(enabled & (1 << Vector)) && Type::isVector();
}

expr SymbolicType::isArray() const {
  return expr(enabled & (1 << Array)) && Type::isArray();
}

void SymbolicType::setName(const string &opname) {
  auto &str = name.empty() ? opname : name;
  i.setName(str);
  f.setName(str);
  p.setName(str);
  a.setName(str);
  v.setName(str);
  Type::setName(str);
}

unsigned SymbolicType::bits() const {
  switch (typ) {
  case Int:    return i.bits();
  case Float:  return f.bits();
  case Ptr:    return p.bits();
  case Array:  return a.bits();
  case Vector: return v.bits();
  case Undefined:
    assert(0 && "undefined at SymbolicType::bits()");
  }
  UNREACHABLE();
}

expr SymbolicType::getTypeConstraints() const {
  expr c(false);
  c |= isInt()    && i.getTypeConstraints();
  c |= isFloat()  && f.getTypeConstraints();
  c |= isPtr()    && p.getTypeConstraints();
  c |= isArray()  && a.getTypeConstraints();
  c |= isVector() && v.getTypeConstraints();
  return c;
}

expr SymbolicType::operator==(const Type &b) const {
  if (auto rhs = dynamic_cast<const IntType*>(&b))
    return isInt() && i == *rhs;
  if (auto rhs = dynamic_cast<const FloatType*>(&b))
    return isFloat() && f == *rhs;
  if (auto rhs = dynamic_cast<const PtrType*>(&b))
    return isPtr() && p == *rhs;
  if (auto rhs = dynamic_cast<const ArrayType*>(&b))
    return isArray() && a == *rhs;
  if (auto rhs = dynamic_cast<const VectorType*>(&b))
    return isVector() && v == *rhs;

  if (auto rhs = dynamic_cast<const SymbolicType*>(&b)) {  
    expr c(false);
    c |= isInt()    && i == rhs->i;
    c |= isFloat()  && f == rhs->f;
    c |= isPtr()    && p == rhs->p;
    c |= isArray()  && a == rhs->a;
    c |= isVector() && v == rhs->v;
    return move(c) && typeVar() == rhs->typeVar();
  }
  assert(0 && "unhandled case in SymbolicType::operator==");
  UNREACHABLE();
}

void SymbolicType::fixup(const Model &m) {
  unsigned smt_typ = m.getUInt(typeVar());
  assert(smt_typ >= Int && smt_typ <= Vector);
  assert((1 << smt_typ) & enabled);
  typ = TypeNum(smt_typ);

  switch (typ) {
  case Int:    i.fixup(m); break;
  case Float:  f.fixup(m); break;
  case Ptr:    p.fixup(m); break;
  case Array:  a.fixup(m); break;
  case Vector: v.fixup(m); break;
  case Undefined:
    UNREACHABLE();
  }
}

void SymbolicType::enforceIntType() {
  enabled &= (1 << Int);
}

void SymbolicType::enforceIntOrPtrOrVectorType() {
  enabled &= (1 << Int) | (1 << Ptr) | (1 << Vector);
}

unique_ptr<Type> SymbolicType::dup() const {
  return make_unique<SymbolicType>(*this);
}

void SymbolicType::print(ostream &os) const {
  if (!name.empty()) {
    os << name;
    return;
  }

  switch (typ) {
  case Int:       i.print(os); break;
  case Float:     f.print(os); break;
  case Ptr:       p.print(os); break;
  case Array:     a.print(os); break;
  case Vector:    v.print(os); break;
  case Undefined: break;
  }
}

}
