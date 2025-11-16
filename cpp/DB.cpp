#include "DB.hpp"
// #include "PreparedStatementHostObject.h"
// #if OP_SQLITE_USE_LIBSQL
// #include "libsql/bridge.h"
// #else
#include "bridge.h"
// #endif
#include "logs.h"
#include "macros.h"
#include "types.hpp"
#include "utils.hpp"
#include <iostream>
#include <jsi/jsi.h>
#include <utility>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

jsi::Object create_db(jsi::Runtime &rt, const std::string &path) {
  auto res = jsi::Object(rt);
  sqlite3 *db = opsqlite_open_v2(path);

  // EXECUTE SYNC
  auto executeSync = HFN(db) {
    const std::string query = args[0].asString(rt).utf8(rt);
    const std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                              ? to_variant_vec(rt, args[1])
                                              : std::vector<JSVariant>();

#ifdef OP_SQLITE_USE_LIBSQL
    auto status = opsqlite_libsql_execute(db, query, &params);
#else
    auto status = opsqlite_execute(db, query, &params);
#endif

    return create_js_rows_2(rt, status);
  });
  res.setProperty(rt, "executeSync", executeSync);

  // EXECUTE
  auto execute = HFN(db) {
    const std::string query = args[0].asString(rt).utf8(rt);
    const std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                              ? to_variant_vec(rt, args[1])
                                              : std::vector<JSVariant>();

    return promisify(
        rt,
        [db, query, params]() {
#ifdef OP_SQLITE_USE_LIBSQL
          return opsqlite_libsql_execute(db, query, &params);
#else
          return opsqlite_execute(db, query, &params);
#endif
        },
        [](jsi::Runtime &rt, std::any results) {
          auto status = std::any_cast<BridgeResult>(results);
          return create_js_rows_2(rt, status);
        });
  });
  res.setProperty(rt, "execute", execute);

  // EXECUTE RAW SYNC
  auto executeRawSync = HFN(db) {
    const std::string query = args[0].asString(rt).utf8(rt);
    const std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                              ? to_variant_vec(rt, args[1])
                                              : std::vector<JSVariant>();

    std::vector<std::vector<JSVariant>> results;
    auto status = opsqlite_execute_raw(db, query, &params, &results);

    return create_raw_result(rt, status, &results);
  });
  res.setProperty(rt, "executeRawSync", executeRawSync);

  // EXECUTE RAW
  auto executeRaw = HFN(db) {
    const std::string query = args[0].asString(rt).utf8(rt);
    const std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                              ? to_variant_vec(rt, args[1])
                                              : std::vector<JSVariant>();

    return promisify(
        rt,
        [db, query, params]() {
          std::vector<std::vector<JSVariant>> results;
          auto status = opsqlite_execute_raw(db, query, &params, &results);
          return std::make_pair(status, results);
        },
        [](jsi::Runtime &rt, std::any result) {
          auto pair = std::any_cast<
              std::pair<BridgeResult, std::vector<std::vector<JSVariant>>>>(
              result);
          return create_raw_result(rt, pair.first, &pair.second);
        });
  });
  res.setProperty(rt, "executeRaw", executeRaw);

  // EXECUTE WITH HOST OBJECTS
  auto executeWithHostObjects = HFN(db) {
    const std::string query = args[0].asString(rt).utf8(rt);
    const std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                              ? to_variant_vec(rt, args[1])
                                              : std::vector<JSVariant>();

    return promisify(
        rt,
        [db, query, params]() {
          std::vector<DumbHostObject> results;
          std::shared_ptr<std::vector<SmartHostObject>> metadata =
              std::make_shared<std::vector<SmartHostObject>>();
          auto status = opsqlite_execute_host_objects(db, query, &params,
                                                      &results, metadata);
          return std::make_tuple(status, results, metadata);
        },
        [](jsi::Runtime &rt, std::any result) {
          auto tuple = std::any_cast<
              std::tuple<BridgeResult, std::vector<DumbHostObject>,
                         std::shared_ptr<std::vector<SmartHostObject>>>>(
              result);
          auto results =
              std::make_shared<std::vector<DumbHostObject>>(std::get<1>(tuple));
          return create_result(rt, std::get<0>(tuple), results.get(),
                               std::get<2>(tuple));
        });
  });
  res.setProperty(rt, "executeWithHostObjects", executeWithHostObjects);

  // EXECUTE BATCH
  auto executeBatch = HFN(db) {
    if (count < 1) {
      throw std::runtime_error(
          "[op-sqlite][executeBatch] Incorrect parameter count");
    }

    const jsi::Value &params = args[0];
    if (params.isNull() || params.isUndefined()) {
      throw std::runtime_error("[op-sqlite][executeBatch] - An array of SQL "
                               "commands or parameters is needed");
    }

    const jsi::Array &batchParams = params.asObject(rt).asArray(rt);
    std::vector<BatchArguments> commands;
    to_batch_arguments(rt, batchParams, &commands);

    return promisify(
        rt, [db, commands]() { return opsqlite_execute_batch(db, &commands); },
        [](jsi::Runtime &rt, std::any result) {
          auto batchResult = std::any_cast<BatchResult>(result);
          auto res = jsi::Object(rt);
          res.setProperty(rt, "rowsAffected",
                          jsi::Value(batchResult.affectedRows));
          return res;
        });
  });
  res.setProperty(rt, "executeBatch", executeBatch);

  // LOAD FILE
  auto loadFile = HFN(db) {
    if (count < 1) {
      throw std::runtime_error(
          "[op-sqlite][loadFile] Incorrect parameter count");
    }

    const std::string sqlFileName = args[0].asString(rt).utf8(rt);

    return promisify(
        rt, [db, sqlFileName]() { return import_sql_file(db, sqlFileName); },
        [](jsi::Runtime &rt, std::any result) {
          auto loadResult = std::any_cast<BatchResult>(result);
          auto res = jsi::Object(rt);
          res.setProperty(rt, "rowsAffected",
                          jsi::Value(loadResult.affectedRows));
          res.setProperty(rt, "commands", jsi::Value(loadResult.commands));
          return res;
        });
  });
  res.setProperty(rt, "loadFile", loadFile);

  // LOAD EXTENSION
  auto loadExtension = HFN(db) {
    auto path = args[0].asString(rt).utf8(rt);
    std::string entry_point;
    if (count > 1 && args[1].isString()) {
      entry_point = args[1].asString(rt).utf8(rt);
    }

    opsqlite_load_extension(db, path, entry_point);
    return {};
  });
  res.setProperty(rt, "loadExtension", loadExtension);

  // ATTACH
  auto attach = HFN(db) {
    auto obj_params = args[0].asObject(rt);
    std::string secondary_db_name =
        obj_params.getProperty(rt, "secondaryDbFileName").asString(rt).utf8(rt);
    std::string alias =
        obj_params.getProperty(rt, "alias").asString(rt).utf8(rt);

    std::string secondary_db_path;
    if (obj_params.hasProperty(rt, "location")) {
      secondary_db_path =
          obj_params.getProperty(rt, "location").asString(rt).utf8(rt);
    }

    opsqlite_attach(db, secondary_db_path, secondary_db_name, alias);
    return {};
  });
  res.setProperty(rt, "attach", attach);

  // DETACH
  auto detach = HFN(db) {
    if (!args[0].isString()) {
      throw std::runtime_error("[op-sqlite] alias must be a string");
    }

    std::string alias = args[0].asString(rt).utf8(rt);
    opsqlite_detach(db, alias);
    return {};
  });
  res.setProperty(rt, "detach", detach);

  // DELETE
  // auto deleteDb = HFN(db) {
  //   std::string db_path = path;

  //   if (count == 1 && args[0].isString()) {
  //     std::string location = args[0].asString(rt).utf8(rt);

  //     if (!location.empty()) {
  //       if (location == ":memory:") {
  //         db_path = ":memory:";
  //       } else if (location.rfind('/', 0) == 0) {
  //         db_path = location;
  //       } else {
  //         // Extract base path from full path
  //         size_t last_slash = path.rfind('/');
  //         std::string base = (last_slash != std::string::npos)
  //                                ? path.substr(0, last_slash + 1)
  //                                : "";
  //         db_path = base + location;
  //       }
  //     }
  //   }

  //   // Extract db name from path
  //   size_t last_slash = db_path.rfind('/');
  //   std::string db_name = (last_slash != std::string::npos)
  //                             ? db_path.substr(last_slash + 1)
  //                             : db_path;

  //   opsqlite_remove(db, db_name, db_path);
  //   return {};
  // });
  // res.setProperty(rt, "delete", deleteDb);

  // PREPARE STATEMENT
  auto prepareStatement = HFN(db) {
    auto query = args[0].asString(rt).utf8(rt);
    sqlite3_stmt *statement = opsqlite_prepare_statement(db, query);

    // Create a simple object with bind and execute methods
    auto stmt_obj = jsi::Object(rt);

    // BIND
    auto bind = HFN2(db, statement) {
      const std::vector<JSVariant> params = count == 1 && args[0].isObject()
                                                ? to_variant_vec(rt, args[0])
                                                : std::vector<JSVariant>();
      opsqlite_bind_statement(statement, &params);
      return {};
    });
    stmt_obj.setProperty(rt, "bind", bind);

    // EXECUTE
    auto executeStmt = HFN2(db, statement) {
      std::vector<DumbHostObject> results;
      std::shared_ptr<std::vector<SmartHostObject>> metadata =
          std::make_shared<std::vector<SmartHostObject>>();

      auto status = opsqlite_execute_prepared_statement(db, statement, &results,
                                                        metadata);

      return create_result(rt, status, &results, metadata);
    });
    stmt_obj.setProperty(rt, "execute", executeStmt);

    return stmt_obj;
  });
  res.setProperty(rt, "prepareStatement", prepareStatement);

  // GET DB PATH
  auto getDbPath = HFN(path) { return jsi::String::createFromUtf8(rt, path); });
  res.setProperty(rt, "getDbPath", getDbPath);

  // CLOSE
  auto close = HFN(db) {
    opsqlite_close(db);
    return {};
  });
  res.setProperty(rt, "close", close);

  return res;
}
} // namespace opsqlite
