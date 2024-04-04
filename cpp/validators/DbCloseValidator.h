//
// Created by jplc on 4/4/24.
//

#ifndef OPSQLITEEXAMPLE_VALIDATORS_DBCLOSEVALIDATOR_H
#define OPSQLITEEXAMPLE_VALIDATORS_DBCLOSEVALIDATOR_H

#include <jsi/jsi.h>
#include <string>

namespace jsi = facebook::jsi;

namespace opsqlite {
namespace validators {

class DbCloseValidator {
public:
  static bool invalidArgsNumber(std::string &msg, int count);
  static bool noStringArgs(std::string &msg, const jsi::Value &arg0);

private:
  DbCloseValidator();
};

}
}

#endif //OPSQLITEEXAMPLE_VALIDATORS_DBCLOSEVALIDATOR_H
