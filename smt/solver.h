#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"
#include <cassert>
#include <cstdint>
#include <functional>
#include <ostream>
#include <utility>

typedef struct _Z3_model* Z3_model;
typedef struct _Z3_solver* Z3_solver;

namespace smt {

class Model {
  Z3_model m;

  Model() : m(0) {}
  Model(Z3_model m);
  ~Model();

  friend class Result;

public:
  Model(Model &&other) : m(0) {
    std::swap(other.m, m);
  }

  void operator=(Model &&other);

  expr operator[](const expr &var) const { return eval(var, true); }
  expr eval(const expr &var, bool complete = false) const;
  uint64_t getUInt(const expr &var) const;
  int64_t getInt(const expr &var) const;

  class iterator {
    Z3_model m;
    unsigned idx;
    iterator(Z3_model m, unsigned idx) : m(m), idx(idx) {}
  public:
    void operator++(void) { ++idx; }
    std::pair<expr, expr> operator*(void) const; // <var, value>
    bool operator!=(const iterator &rhs) const { return idx != rhs.idx; }
    friend class Model;
  };

  // WARNING: the parent Model class has to be alive while iterators are in use.
  iterator begin() const;
  iterator end() const;

  friend std::ostream& operator<<(std::ostream &os, const Model &m);
};


class Result {
public:
  enum answer { UNSAT, SAT, INVALID, UNKNOWN };

  Result() : a(UNKNOWN) {}

  bool isSat() const { return a == SAT; }
  bool isUnsat() const { return a == UNSAT; }
  bool isInvalid() const { return a == INVALID; }
  bool isUnknown() const { return a == UNKNOWN; }

  const Model& getModel() const {
    assert(isSat());
    return m;
  }

private:
  Model m;
  answer a;

  Result(answer a) : a(a) {}
  Result(Z3_model m) : m(m), a(SAT) {}

  friend class Solver;
};


class Solver;

class SolverPop {
  Solver &s;
  SolverPop(Solver &s) : s(s) {}
public:
  ~SolverPop();
  friend class Solver;
};


class Solver {
  Z3_solver s;
  bool valid = true;
  using E = std::pair<expr, std::function<void(const Result &r)>>;
public:
  Solver();
  ~Solver();

  void add(const expr &e);
  void block(const Model &m);
  SolverPop push();
  void reset();

  Result check() const;
  static void check(std::initializer_list<E> queries);

  friend class SolverPop;
};


void solver_print_queries(bool yes);
void solver_tactic_verbose(bool yes);
void solver_print_stats(std::ostream &os);


struct EnableSMTQueriesTMP {
  bool old;
  EnableSMTQueriesTMP();
  ~EnableSMTQueriesTMP();
};


void solver_init();
void solver_destroy();

}
