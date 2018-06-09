#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <ostream>
#include <string>
#include <vector>

namespace util {

class Errors {
  std::vector<std::string> errs;

public:
  void add(const char *str);
  explicit operator bool() const { return !errs.empty(); }

  friend std::ostream& operator<<(std::ostream &os, const Errors &e);
};

}
