//
// Created by jplc on 4/4/24.
//

#include "DbCloseValidator.h"

namespace opsqlite {
namespace validators {

bool DbCloseValidator::invalidArgsNumber(std::string &msg, int count) {
  if (count == 0) {
    msg = "[op-sqlite][close] database name is required";
    return true;
  }
  return false;
}

bool DbCloseValidator::noStringArgs(std::string &msg, const jsi::Value &arg0) {
  if (!arg0.isString()) {
    msg = "[op-sqlite][close] database name must be a string";
    return true;
  }
  return false;
}

}
}