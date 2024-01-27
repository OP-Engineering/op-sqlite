//
//  PreparedStatementHostObject.cpp
//  op-sqlite
//
//  Created by Oscar Franco on 5/12/23.
//

#include "PreparedStatementHostObject.h"
#include "bridge.h"
#include "macros.h"
#include "utils.h"

namespace opsqlite {

namespace jsi = facebook::jsi;

PreparedStatementHostObject::PreparedStatementHostObject(
    std::string dbName, sqlite3_stmt *statementPtr)
    : _dbName(dbName), _statement(statementPtr) {}

std::vector<jsi::PropNameID>
PreparedStatementHostObject::getPropertyNames(jsi::Runtime &rt) {
  std::vector<jsi::PropNameID> keys;

  return keys;
}

jsi::Value PreparedStatementHostObject::get(jsi::Runtime &rt,
                                            const jsi::PropNameID &propNameID) {
  auto name = propNameID.utf8(rt);

  if (name == "bind") {
    return HOSTFN("bind", 1) {
      if (_statement == NULL) {
        throw std::runtime_error("statement has been freed");
      }

      const jsi::Value &js_params = args[0];
      std::vector<JSVariant> params = toVariantVec(rt, js_params);

      opsqlite_bind_statement(_statement, &params);

      return {};
    });
  }

  if (name == "execute") {
    return HOSTFN("execute", 1) {
      if (_statement == NULL) {
        throw std::runtime_error("statement has been freed");
      }
      std::vector<DumbHostObject> results;
      std::shared_ptr<std::vector<SmartHostObject>> metadata =
          std::make_shared<std::vector<SmartHostObject>>();

      auto status = opsqlite_execute_prepared_statement(_dbName, _statement,
                                                        &results, metadata);

      if (status.type == SQLiteError) {
        throw std::runtime_error(status.message);
      }

      auto jsiResult = createResult(rt, status, &results, metadata);
      return jsiResult;
    });
  }

  return {};
}

PreparedStatementHostObject::~PreparedStatementHostObject() {
  if (_statement != NULL) {
    sqlite3_finalize(_statement);
    _statement = NULL;
  }
}

} // namespace opsqlite
