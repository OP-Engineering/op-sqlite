//
//  PreparedStatementHostObject.cpp
//  op-sqlite
//
//  Created by Oscar Franco on 5/12/23.
//

#include "PreparedStatementHostObject.h"
#include "utils.h"

namespace opsqlite {

namespace jsi = facebook::jsi;

PreparedStatementHostObject::PreparedStatementHostObject(
    sqlite3_stmt *statementPtr)
    : _statement(statementPtr) {}

std::vector<jsi::PropNameID>
PreparedStatementHostObject::getPropertyNames(jsi::Runtime &rt) {
  std::vector<jsi::PropNameID> keys;

  //  for (auto field : fields) {
  //    keys.push_back(jsi::PropNameID::forAscii(rt, field.first));
  //  }

  return keys;
}

jsi::Value PreparedStatementHostObject::get(jsi::Runtime &rt,
                                            const jsi::PropNameID &propNameID) {
  //  auto name = propNameID.utf8(rt);
  //
  //  for (auto field : fields) {
  //    auto fieldName = field.first;
  //    if (fieldName == name) {
  //      return toJSI(rt, field.second);
  //    }
  //  }

  return {};
}

} // namespace opsqlite
