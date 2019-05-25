#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state_value.h"
#include "smt/expr.h"

namespace IR {

class Memory;
class State;

class Pointer {
  Memory &m;
  // [offset, local_bid, nonlocal_bid]
  smt::expr p;

  unsigned bits_for_bids() const;

public:
  Pointer(Memory &m, smt::expr p) : m(m), p(std::move(p)) {}
  Pointer(Memory &m, unsigned bid, bool local);

  smt::expr get_bid() const;
  smt::expr get_offset() const;

  const smt::expr& operator()() const { return p; }

  void operator++(void);

  void is_dereferenceable(unsigned bytes);
};


class Memory {
  State &state;

  // FIXME: these should be tuned per function
  unsigned bits_for_offset = 32;
  unsigned bits_for_local_bid = 8;
  unsigned bits_for_nonlocal_bid = 8;
  unsigned bits_size_t = 64;
  unsigned byte_size = 8;

  smt::expr blocks_size; // array: bid -> size in bytes
  smt::expr blocks_val;  // array: (bid, offset) -> StateValue
  unsigned last_bid = 0;

public:
  Memory(State &state);

  smt::expr alloc(const smt::expr &bytes, bool local);
  void store(const smt::expr &ptr, const StateValue &val);
  StateValue load(const smt::expr &ptr, unsigned bits);

  void memset(const smt::expr &ptr, const StateValue &val,
              const smt::expr &bytes);
  void memcpy(const smt::expr &dst, const smt::expr &src,
              const smt::expr &bytes);

  static Memory ite(const smt::expr &cond, const Memory &then,
                    const Memory &els);

  friend class Pointer;
};

}
