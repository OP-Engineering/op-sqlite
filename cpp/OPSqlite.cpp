#include "OPSqlite.hpp"
#include "DumbHostObject.h"
#include "OPThreadPool.h"
#include "OPDB.hpp"
#include "PreparedStatementHostObject.h"
#ifdef OP_SQLITE_USE_LIBSQL
#include "libsql/OPBridge.hpp"
#else
#include "OPBridge.hpp"
#endif
#include "logs.h"
#include "macros.hpp"
#include "utils.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

std::string _base_path;
std::string _sqlite_vec_path;
std::vector<std::shared_ptr<OPDB>> native_dbs;
bool invalidated = false;
std::shared_ptr<react::CallInvoker> invoker;

// React native will try to clean the module on JS context invalidation
// (CodePush/Hot Reload) The clearState function is called
void invalidate() {
  // Global flag used by the threads to stop work
  invalidated = true;

  for (const auto &db : native_dbs) {
    db->invalidate();
  }

  // Clear our existing vector of shared pointers so they can be garbage
  // collected
  native_dbs.clear();
}

jsi::Object create_db(jsi::Runtime &rt, const std::shared_ptr<OPDB> &opdb) {
  jsi::Object db(rt);
  native_dbs.emplace_back(opdb);
  db.setNativeState(rt, opdb);

  db.setProperty(rt, "attach", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    std::string secondary_db_path = std::string(opdb->base_path);

    auto obj_params = args[0].asObject(rt);

    std::string secondary_db_name =
        obj_params.getProperty(rt, "secondaryDbFileName").asString(rt).utf8(rt);
    std::string alias =
        obj_params.getProperty(rt, "alias").asString(rt).utf8(rt);

    if (obj_params.hasProperty(rt, "location")) {
      std::string location =
          obj_params.getProperty(rt, "location").asString(rt).utf8(rt);
      secondary_db_path = secondary_db_path + location;
    }

    if (secondary_db_name.find('\0') != std::string::npos) {
      throw std::runtime_error(
          "[op-sqlite] attach secondaryDbFileName must not contain a zero byte");
    }
    if (alias.find('\0') != std::string::npos) {
      throw std::runtime_error(
          "[op-sqlite] attach alias must not contain a zero byte");
    }

#ifdef OP_SQLITE_USE_LIBSQL
    opsqlite_libsql_attach(opdb->db, secondary_db_path, secondary_db_name,
                           alias);
#else
    opsqlite_attach(opdb->db, secondary_db_path, secondary_db_name, alias);
#endif

    return {};
  }));

  db.setProperty(rt, "detach", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    if (!args[0].isString()) {
      throw std::runtime_error("[op-sqlite] alias must be a strings");
    }

    std::string alias = args[0].asString(rt).utf8(rt);
    if (alias.find('\0') != std::string::npos) {
      throw std::runtime_error(
          "[op-sqlite] detach alias must not contain a zero byte");
    }
#ifdef OP_SQLITE_USE_LIBSQL
    opsqlite_libsql_detach(opdb->db, alias);
#else
    opsqlite_detach(opdb->db, alias);
#endif

    return {};
  }));

  db.setProperty(rt, "close", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    opdb->invalidated = true;
#if !defined(OP_SQLITE_USE_LIBSQL) && !defined(OP_SQLITE_USE_TURSO)
    if (opdb->db != nullptr) {
      sqlite3_interrupt(opdb->db);
    }
#endif
    opdb->thread_pool->waitFinished();
#ifdef OP_SQLITE_USE_LIBSQL
    opsqlite_libsql_close(opdb->db);
    opdb->db = {};
#else
    opsqlite_close(opdb->db);
    opdb->db = nullptr;
#endif

    return {};
  }));

  db.setProperty(rt, "interrupt", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    if (opdb->invalidated) {
      throw std::runtime_error("[op-sqlite][interrupt] database is closed");
    }

#ifdef OP_SQLITE_USE_LIBSQL
    throw std::runtime_error("[op-sqlite][interrupt] sqlite3_interrupt is not "
                             "supported with libsql");
#elif defined(OP_SQLITE_USE_TURSO)
    throw std::runtime_error("[op-sqlite][interrupt] sqlite3_interrupt is not "
                             "supported with Turso");
#else
    if (opdb->db == nullptr) {
      throw std::runtime_error("[op-sqlite][interrupt] database is null");
    }

    sqlite3_interrupt(opdb->db);
    return {};
#endif
  }));

  db.setProperty(rt, "delete", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    if (count != 0) {
      throw std::runtime_error("[op-sqlite] Delete no longer takes arguments");
    }

    opdb->invalidated = true;
#if !defined(OP_SQLITE_USE_LIBSQL) && !defined(OP_SQLITE_USE_TURSO)
    if (opdb->db != nullptr) {
      sqlite3_interrupt(opdb->db);
    }
#endif
    opdb->thread_pool->waitFinished();

    if (opdb->delete_db_name.empty()) {
      throw std::runtime_error(
          "[op-sqlite][delete] delete() is not supported for remote-only databases");
    }

#ifdef OP_SQLITE_USE_LIBSQL
    opsqlite_libsql_remove(opdb->db, opdb->delete_db_name, opdb->base_path);
#else
    opsqlite_remove(opdb->db, opdb->delete_db_name, opdb->base_path);
#endif

    return {};
  }));

  db.setProperty(rt, "executeRaw", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    const std::string query = args[0].asString(rt).utf8(rt);
    const std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                              ? to_variant_vec(rt, args[1])
                                              : std::vector<JSVariant>();

    return promisify(
        rt, opdb->thread_pool,
        [opdb, query, params]() {
          std::vector<std::vector<JSVariant>> results;
#ifdef OP_SQLITE_USE_LIBSQL
          auto status =
              opsqlite_libsql_execute_raw(opdb->db, query, &params, &results);
#else
          auto status = opsqlite_execute_raw(opdb->db, query, &params, &results);
#endif
          return std::make_tuple(status, results);
        },
        [](jsi::Runtime &rt, std::any prev) {
          auto tuple = std::any_cast<
              std::tuple<BridgeResult, std::vector<std::vector<JSVariant>>>>(
              prev);

          return create_raw_result(rt, std::get<0>(tuple), &std::get<1>(tuple));
        });
  }));

  db.setProperty(rt, "executeSync", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params;

    if (count == 2) {
      params = to_variant_vec(rt, args[1]);
    }
#ifdef OP_SQLITE_USE_LIBSQL
    auto status = opsqlite_libsql_execute(opdb->db, query, &params);
#else
    auto status = opsqlite_execute(opdb->db, query, &params);
#endif

    return create_js_rows(rt, status);
  }));

  db.setProperty(rt, "executeRawSync", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    const std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                        ? to_variant_vec(rt, args[1])
                                        : std::vector<JSVariant>();

    std::vector<std::vector<JSVariant>> results;

#ifdef OP_SQLITE_USE_LIBSQL
    auto status =
        opsqlite_libsql_execute_raw(opdb->db, query, &params, &results);
#else
    auto status = opsqlite_execute_raw(opdb->db, query, &params, &results);
#endif

    return create_raw_result(rt, status, &results);
  }));

  db.setProperty(rt, "execute", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    const std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                        ? to_variant_vec(rt, args[1])
                                        : std::vector<JSVariant>();

    return promisify(
        rt, opdb->thread_pool,
        [opdb, query, params]() {
#ifdef OP_SQLITE_USE_LIBSQL
          auto status = opsqlite_libsql_execute(opdb->db, query, &params);
#else
          auto status = opsqlite_execute(opdb->db, query, &params);
#endif
          return status;
        },
        [](jsi::Runtime &rt, std::any prev) {
          auto status = std::any_cast<BridgeResult>(prev);
          return create_js_rows(rt, status);
        });
  }));

  db.setProperty(rt, "executeWithHostObjects", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    const std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                        ? to_variant_vec(rt, args[1])
                                        : std::vector<JSVariant>();

    return promisify(
        rt, opdb->thread_pool,
        [opdb, query, params]() {
          std::vector<DumbHostObject> results;
          std::shared_ptr<std::vector<SmartHostObject>> metadata =
              std::make_shared<std::vector<SmartHostObject>>();
#ifdef OP_SQLITE_USE_LIBSQL
          auto status = opsqlite_libsql_execute_with_host_objects(
              opdb->db, query, &params, &results, metadata);
#else
          auto status = opsqlite_execute_host_objects(opdb->db, query, &params,
                                                      &results, metadata);
#endif
          return std::make_tuple(status, results, metadata);
        },
        [](jsi::Runtime &rt, std::any prev) {
          auto tuple = std::any_cast<
              std::tuple<BridgeResult, std::vector<DumbHostObject>,
                         std::shared_ptr<std::vector<SmartHostObject>>>>(prev);
          auto results =
              std::make_shared<std::vector<DumbHostObject>>(std::get<1>(tuple));
          return create_result(rt, std::get<0>(tuple), results.get(),
                               std::get<2>(tuple));
        });
  }));

  db.setProperty(rt, "executeBatch", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    if (count < 1) {
      throw std::runtime_error(
          "[op-sqlite][executeAsyncBatch] Incorrect parameter count");
    }

    const jsi::Value &params = args[0];

    if (params.isNull() || params.isUndefined()) {
      throw std::runtime_error(
          "[op-sqlite][executeAsyncBatch] - An array of SQL commands or parameters is needed");
    }

    const jsi::Array &batchParams = params.asObject(rt).asArray(rt);

    std::vector<BatchArguments> commands;
    to_batch_arguments(rt, batchParams, &commands);

    return promisify(
        rt, opdb->thread_pool,
        [opdb, commands]() {
#ifdef OP_SQLITE_USE_LIBSQL
          auto batchResult = opsqlite_libsql_execute_batch(opdb->db, &commands);
#else
          auto batchResult = opsqlite_execute_batch(opdb->db, &commands);
#endif
          return batchResult;
        },
        [](jsi::Runtime &rt, std::any prev) {
          auto batchResult = std::any_cast<BatchResult>(prev);
          auto res = jsi::Object(rt);
          res.setProperty(rt, "rowsAffected",
                          jsi::Value(batchResult.affectedRows));
          return res;
        });
  }));

#if defined(OP_SQLITE_USE_LIBSQL) || defined(OP_SQLITE_USE_TURSO)
  db.setProperty(rt, "sync", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
#ifdef OP_SQLITE_USE_LIBSQL
    opsqlite_libsql_sync(opdb->db);
#else
    opsqlite_sync(opdb->db);
#endif
    return {};
  }));

#ifdef OP_SQLITE_USE_LIBSQL
  db.setProperty(rt, "setReservedBytes", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    auto reserved_bytes = static_cast<int32_t>(args[0].asNumber());
    opsqlite_libsql_set_reserved_bytes(opdb->db, reserved_bytes);
    return {};
  }));

  db.setProperty(rt, "getReservedBytes", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    return {opsqlite_libsql_get_reserved_bytes(opdb->db)};
  }));
#elif defined(OP_SQLITE_USE_TURSO)
  db.setProperty(rt, "setReservedBytes", HFN0 {
    throw std::runtime_error(
        "[op-sqlite] Function setReservedBytes not implemented for current backend (libsql or sqlcipher)");
  }));

  db.setProperty(rt, "getReservedBytes", HFN0 {
    throw std::runtime_error(
        "[op-sqlite] Function getReservedBytes not implemented for current backend (libsql or sqlcipher)");
  }));
#endif
#else
  db.setProperty(rt, "sync", HFN0 {
    throw std::runtime_error(
        "[op-sqlite] Function sync not implemented for current backend (libsql or sqlcipher)");
  }));

  db.setProperty(rt, "setReservedBytes", HFN0 {
    throw std::runtime_error(
        "[op-sqlite] Function setReservedBytes not implemented for current backend (libsql or sqlcipher)");
  }));

  db.setProperty(rt, "getReservedBytes", HFN0 {
    throw std::runtime_error(
        "[op-sqlite] Function getReservedBytes not implemented for current backend (libsql or sqlcipher)");
  }));
#endif

#if !defined(OP_SQLITE_USE_LIBSQL) && !defined(OP_SQLITE_USE_TURSO)
  db.setProperty(rt, "loadFile", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    if (count < 1) {
      throw std::runtime_error(
          "[op-sqlite][loadFile] Incorrect parameter count");
    }

    const std::string sqlFileName = args[0].asString(rt).utf8(rt);

    return promisify(
        rt, opdb->thread_pool,
        [opdb, sqlFileName]() { return import_sql_file(opdb->db, sqlFileName); },
        [](jsi::Runtime &rt, std::any prev) {
          auto result = std::any_cast<BatchResult>(prev);
          auto res = jsi::Object(rt);
          res.setProperty(rt, "rowsAffected", jsi::Value(result.affectedRows));
          res.setProperty(rt, "commands", jsi::Value(result.commands));
          return res;
        });
  }));

  db.setProperty(rt, "updateHook", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    auto callback = std::make_shared<jsi::Value>(rt, args[0]);

    if (callback->isUndefined() || callback->isNull()) {
      opdb->update_hook_callback = nullptr;
    } else {
      opdb->update_hook_callback = callback;
    }

    opdb->auto_register_update_hook();
    return {};
  }));

  db.setProperty(rt, "commitHook", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    if (count < 1) {
      throw std::runtime_error("[op-sqlite][commitHook] callback needed");
    }

    auto callback = std::make_shared<jsi::Value>(rt, args[0]);
    if (callback->isUndefined() || callback->isNull()) {
      opdb->commit_hook_callback = nullptr;
      opdb->deregister_commit_hook();
      return {};
    }
    opdb->commit_hook_callback = callback;
    opdb->register_commit_hook();

    return {};
  }));

  db.setProperty(rt, "rollbackHook", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    if (count < 1) {
      throw std::runtime_error("[op-sqlite][rollbackHook] callback needed");
    }

    auto callback = std::make_shared<jsi::Value>(rt, args[0]);

    if (callback->isUndefined() || callback->isNull()) {
      opdb->rollback_hook_callback = nullptr;
      opdb->deregister_rollback_hook();
      return {};
    }
    opdb->rollback_hook_callback = callback;

    opdb->register_rollback_hook();
    return {};
  }));

  db.setProperty(rt, "loadExtension", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    auto path = args[0].asString(rt).utf8(rt);
    std::string entry_point;
    if (count > 1 && args[1].isString()) {
      entry_point = args[1].asString(rt).utf8(rt);
    }

    opsqlite_load_extension(opdb->db, path, entry_point);
    return {};
  }));

  db.setProperty(rt, "reactiveExecute", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    auto query = args[0].asObject(rt);

    const std::string query_str =
        query.getProperty(rt, "query").asString(rt).utf8(rt);
    auto js_args = query.getProperty(rt, "arguments");
    auto js_discriminators =
        query.getProperty(rt, "fireOn").asObject(rt).asArray(rt);
    auto variant_args = to_variant_vec(rt, js_args);

    sqlite3_stmt *stmt = opsqlite_prepare_statement(opdb->db, query_str);
    opsqlite_bind_statement(stmt, &variant_args);

    auto callback =
        std::make_shared<jsi::Value>(query.getProperty(rt, "callback"));

    std::vector<OPTableRowDiscriminator> discriminators;

    for (size_t i = 0; i < js_discriminators.length(rt); i++) {
      auto js_discriminator =
          js_discriminators.getValueAtIndex(rt, i).asObject(rt);
      std::string table =
          js_discriminator.getProperty(rt, "table").asString(rt).utf8(rt);
      std::vector<int> ids;
      if (js_discriminator.hasProperty(rt, "ids")) {
        auto js_ids =
            js_discriminator.getProperty(rt, "ids").asObject(rt).asArray(rt);
        for (size_t j = 0; j < js_ids.length(rt); j++) {
          ids.push_back(
              static_cast<int>(js_ids.getValueAtIndex(rt, j).asNumber()));
        }
      }
      discriminators.push_back({table, ids});
    }

    std::shared_ptr<OPReactiveQuery> reactiveQuery =
        std::make_shared<OPReactiveQuery>(
            OPReactiveQuery{stmt, discriminators, callback});

    opdb->reactive_queries.push_back(reactiveQuery);

    opdb->auto_register_update_hook();

    auto unsubscribe = HFN2(opdb, reactiveQuery) {
      auto it = std::find(opdb->reactive_queries.begin(),
                          opdb->reactive_queries.end(), reactiveQuery);
      if (it != opdb->reactive_queries.end()) {
        opdb->reactive_queries.erase(it);
      }
      opdb->auto_register_update_hook();
      return {};
    });

    return unsubscribe;
  }));
#else
  db.setProperty(rt, "loadFile", HFN0 {
    throw std::runtime_error(
        "[op-sqlite] Function loadFile not implemented for current backend (libsql or sqlcipher)");
  }));

  db.setProperty(rt, "updateHook", HFN0 {
    throw std::runtime_error(
        "[op-sqlite] Function updateHook not implemented for current backend (libsql or sqlcipher)");
  }));

  db.setProperty(rt, "commitHook", HFN0 {
    throw std::runtime_error(
        "[op-sqlite] Function commitHook not implemented for current backend (libsql or sqlcipher)");
  }));

  db.setProperty(rt, "rollbackHook", HFN0 {
    throw std::runtime_error(
        "[op-sqlite] Function rollbackHook not implemented for current backend (libsql or sqlcipher)");
  }));

  db.setProperty(rt, "loadExtension", HFN0 {
    throw std::runtime_error(
        "[op-sqlite] Function loadExtension not implemented for current backend (libsql or sqlcipher)");
  }));

  db.setProperty(rt, "reactiveExecute", HFN0 {
    throw std::runtime_error(
        "[op-sqlite] Function reactiveExecute not implemented for current backend (libsql or sqlcipher)");
  }));
#endif

  db.setProperty(rt, "prepareStatement", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    auto query = args[0].asString(rt).utf8(rt);
#ifdef OP_SQLITE_USE_LIBSQL
    libsql_stmt_t statement = opsqlite_libsql_prepare_statement(opdb->db, query);
#else
    sqlite3_stmt *statement = opsqlite_prepare_statement(opdb->db, query);
#endif
    auto preparedStatementHostObject =
        std::make_shared<PreparedStatementHostObject>(opdb->db, statement,
                                                      opdb->thread_pool);

    return jsi::Object::createFromHostObject(rt, preparedStatementHostObject);
  }));

  db.setProperty(rt, "getDbPath", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    std::string path = std::string(opdb->base_path);

    if (count == 1) {
      if (!args[0].isString()) {
        throw std::runtime_error(
            "[op-sqlite][open] database location must be a string");
      }

      std::string last_path = args[0].asString(rt).utf8(rt);

      if (last_path == ":memory:") {
        path = ":memory:";
      } else if (last_path.rfind('/', 0) == 0) {
        path = last_path;
      } else {
        path = path + "/" + last_path;
      }
    }

    auto result = opsqlite_get_db_path(opdb->db_name, path);
    return jsi::String::createFromUtf8(rt, result);
  }));

  db.setProperty(rt, "flushPendingReactiveQueries", HFN0 {
    auto opdb = that.asObject(rt).getNativeState<OPDB>(rt);
    auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
    auto promise = promiseCtr.callAsConstructor(rt, HFN(opdb) {
      auto resolve = std::make_shared<jsi::Value>(rt, args[0]);

      auto task = [opdb, resolve]() {
        opdb->flush_pending_reactive_queries(resolve);
      };

      opdb->thread_pool->queueWork(task);

      return {};
    }));

    return promise;
  }));

  return db;
}

jsi::Object create_db(jsi::Runtime &rt, const std::string &name,
                      const std::string &path,
                      const std::string &sqlite_vec_path,
                      const std::string &encryption_key) {
  return create_db(
      rt, std::make_shared<OPDB>(name, path, sqlite_vec_path, encryption_key));
}


void install(jsi::Runtime &rt,
             const std::shared_ptr<react::CallInvoker> &_invoker,
             const char *base_path, const char *sqlite_vec_path) {

  _base_path = std::string(base_path);
  _sqlite_vec_path = std::string(sqlite_vec_path);
  opsqlite::invoker = _invoker;
  opsqlite::invalidated = false;
  
  auto open = HFN0 {
    jsi::Object options = args[0].asObject(rt);
    std::string name = options.getProperty(rt, "name").asString(rt).utf8(rt);
    std::string path = std::string(_base_path);
    std::string location;
    std::string encryption_key;
    
    if (options.hasProperty(rt, "location")) {
      location = options.getProperty(rt, "location").asString(rt).utf8(rt);
    }
    
    if (options.hasProperty(rt, "encryptionKey")) {
      encryption_key =
      options.getProperty(rt, "encryptionKey").asString(rt).utf8(rt);
    }
    
    if (!location.empty()) {
      if (location == ":memory:") {
        path = ":memory:";
      } else if (location.rfind('/', 0) == 0) {
        path = location;
      } else {
        path = path + "/" + location;
      }
    }
    
    return create_db(rt, name, path, _sqlite_vec_path, encryption_key);
  });

  auto is_sqlcipher = HFN(=) {
#ifdef OP_SQLITE_USE_SQLCIPHER
    return true;
#else
    return false;
#endif
  });

  auto is_ios_embedded = HFN(=) {
#ifdef OP_SQLITE_USE_PHONE_VERSION
    return true;
#else
    return false;
#endif
  });

  auto is_libsql = HFN(=) {
#ifdef OP_SQLITE_USE_LIBSQL
    return true;
#else
    return false;
#endif
  });

  auto is_turso = HFN(=) {
#ifdef OP_SQLITE_USE_TURSO
    return true;
#else
    return false;
#endif
  });

#if defined(OP_SQLITE_USE_LIBSQL) || defined(OP_SQLITE_USE_TURSO)
  auto open_remote = HFN(=) {
    jsi::Object options = args[0].asObject(rt);

    std::string url = options.getProperty(rt, "url").asString(rt).utf8(rt);

    std::string auth_token =
        options.getProperty(rt, "authToken").asString(rt).utf8(rt);

#ifdef OP_SQLITE_USE_LIBSQL
    auto db = std::make_shared<OPDB>(url, auth_token);
#else
    std::string path = std::string(_base_path);
    auto db = std::make_shared<OPDB>(url, auth_token, path);
#endif

    return create_db(rt, db);
  });

  auto open_sync = HFN(=) {
    jsi::Object options = args[0].asObject(rt);
    std::string name = options.getProperty(rt, "name").asString(rt).utf8(rt);
    std::string path = std::string(_base_path);
    std::string url = options.getProperty(rt, "url").asString(rt).utf8(rt);
    std::string auth_token =
        options.getProperty(rt, "authToken").asString(rt).utf8(rt);

    int sync_interval = 0;
    if (options.hasProperty(rt, "libsqlSyncInterval")) {
      sync_interval = static_cast<int>(
          options.getProperty(rt, "libsqlSyncInterval").asNumber());
    }

    bool offline = false;
    if (options.hasProperty(rt, "libsqlOffline")) {
      offline = options.getProperty(rt, "libsqlOffline").asBool();
    }

    std::string encryption_key;
    if (options.hasProperty(rt, "encryptionKey")) {
      encryption_key =
          options.getProperty(rt, "encryptionKey").asString(rt).utf8(rt);
    }

    std::string remote_encryption_key;
    if (options.hasProperty(rt, "remoteEncryptionKey")) {
      remote_encryption_key =
          options.getProperty(rt, "remoteEncryptionKey").asString(rt).utf8(rt);
    }

    std::string location;
    if (options.hasProperty(rt, "location")) {
      location = options.getProperty(rt, "location").asString(rt).utf8(rt);
    }
    if (!location.empty()) {
      if (location == ":memory:") {
        path = ":memory:";
      } else if (location.rfind('/', 0) == 0) {
        path = location;
      } else {
        path = path + "/" + location;
      }
    }

  #ifdef OP_SQLITE_USE_LIBSQL
    auto db = std::make_shared<OPDB>(
      name, path, url, auth_token, sync_interval, offline, encryption_key,
      remote_encryption_key);
  #else
    (void)sync_interval;
    (void)offline;

    auto db = std::make_shared<OPDB>(
      name, path, url, auth_token, remote_encryption_key);
  #endif

    return create_db(rt, db);
  });
#endif

  jsi::Object module = jsi::Object(rt);
  module.setProperty(rt, "open", std::move(open));
  module.setProperty(rt, "isSQLCipher", std::move(is_sqlcipher));
  module.setProperty(rt, "isLibsql", std::move(is_libsql));
  module.setProperty(rt, "isTurso", std::move(is_turso));
  module.setProperty(rt, "isIOSEmbedded", std::move(is_ios_embedded));
#if defined(OP_SQLITE_USE_LIBSQL) || defined(OP_SQLITE_USE_TURSO)
  module.setProperty(rt, "openRemote", std::move(open_remote));
  module.setProperty(rt, "openSync", std::move(open_sync));
#endif

  rt.global().setProperty(rt, "__OPSQLiteProxy", std::move(module));
}

void expoUpdatesWorkaround(const char *base_path) {
#ifdef OP_SQLITE_USE_LIBSQL
  std::string path = std::string(base_path);
  // Open a DB before anything else so that expo-updates does not mess up the
  // configuration
  opsqlite_libsql_open("__dummy", path, "");
#endif
}

} // namespace opsqlite
