//
// Created by jplc on 4/4/24.
//

#include "DbDetachValidator.h"

namespace opsqlite {
namespace validators {

bool DbDetachValidator::invalidArgsNumber(std::string &msg, int count) {
  if (count < 2) {
    msg = "[op-sqlite][detach] Incorrect number of arguments";
    return true;
  }
  return false;
}

bool DbDetachValidator::noStringArgs(std::string &msg, const jsi::Value &arg0, const jsi::Value &arg1) {
  if (!arg0.isString() || !arg1.isString()) {
    msg = "dbName, databaseToAttach and alias must be a strings";
    return true;
  }
  return false;
}

}
}