//
// Created by jplc on 4/4/24.
//

#include "DbAttachValidator.h"

namespace opsqlite {
namespace validators {

bool DbAttachValidator::invalidArgsNumber(std::string &msg, int count) {
  if (count < 3) {
    msg = "[op-sqlite][attach] Incorrect number of arguments";
    return true;
  }
  return false;
}

bool DbAttachValidator::noStringArgs(std::string &msg, const jsi::Value &arg1, const jsi::Value &arg2, const jsi::Value &arg3) {
  if (!arg1.isString() || !arg2.isString() || !arg3.isString()) {
    msg = "dbName, databaseToAttach and alias must be a strings";
    return true;
  }
  return false;
}

bool DbAttachValidator::locationArgDefined(const int count, const jsi::Value &arg) {
  return count > 3 && !arg.isUndefined() && !arg.isNull();
}

bool DbAttachValidator::locationArgIsNotString(std::string &msg, const jsi::Value &arg) {
  if (!arg.isString()) {
    msg = "[op-sqlite][attach] database location must be a string";
    return true;
  }
  return false;
}

}
}