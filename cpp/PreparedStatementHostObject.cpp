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

  //  for (auto field : fields) {
  //    keys.push_back(jsi::PropNameID::forAscii(rt, field.first));
  //  }

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

      std::vector<JSVariant> params;

      const jsi::Value &originalParams = args[0];
      params = toVariantVec(rt, originalParams);

      std::vector<DumbHostObject> results;
      std::shared_ptr<std::vector<SmartHostObject>> metadata =
          std::make_shared<std::vector<SmartHostObject>>();

      sqlite_bind_statement(_statement, &params);

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

      auto status = sqlite_execute_prepared_statement(_dbName, _statement,
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
