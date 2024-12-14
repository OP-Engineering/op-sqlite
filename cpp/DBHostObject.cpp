#include "DBHostObject.h"
#include "PreparedStatementHostObject.h"
#if OP_SQLITE_USE_LIBSQL
#include "libsql/bridge.h"
#else
#include "bridge.h"
#endif
#include "logs.h"
#include "macros.h"
#include "utils.h"
#include <iostream>
#include <utility>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

#ifdef OP_SQLITE_USE_LIBSQL
void DBHostObject::flush_pending_reactive_queries(
    std::shared_ptr<jsi::Value> resolve) {
  invoker->invokeAsync(
      [this, resolve]() { resolve->asObject(rt).asFunction(rt).call(rt, {}); });
}
#else
void DBHostObject::flush_pending_reactive_queries(
    std::shared_ptr<jsi::Value> resolve) {
  for (const auto &query_ptr : pending_reactive_queries) {
    auto query = query_ptr.get();

    std::vector<DumbHostObject> results;
    std::shared_ptr<std::vector<SmartHostObject>> metadata =
        std::make_shared<std::vector<SmartHostObject>>();

    auto status = opsqlite_execute_prepared_statement(db, query->stmt,
                                                      &results, metadata);

    if (status.type == SQLiteError) {
      invoker->invokeAsync(
          [this, callback = query->callback, status = std::move(status)] {
            auto errorCtr = rt.global().getPropertyAsFunction(rt, "Error");
            auto error = errorCtr.callAsConstructor(
                rt, jsi::String::createFromUtf8(rt, status.message));
            callback->asObject(rt).asFunction(rt).call(rt, error);
          });
    } else {
      invoker->invokeAsync(
          [this,
           results = std::make_shared<std::vector<DumbHostObject>>(results),
           callback = query->callback, metadata, status = std::move(status)] {
            auto jsiResult = create_result(rt, status, results.get(), metadata);
            callback->asObject(rt).asFunction(rt).call(rt, jsiResult);
          });
    }
  }

  pending_reactive_queries.clear();

  invoker->invokeAsync(
      [this, resolve]() { resolve->asObject(rt).asFunction(rt).call(rt, {}); });
}

void DBHostObject::auto_register_update_hook() {
  if (update_hook_callback == nullptr && reactive_queries.empty() &&
      is_update_hook_registered) {
    opsqlite_deregister_update_hook(db_name);
    is_update_hook_registered = false;
    return;
  }

  if (is_update_hook_registered) {
    return;
  }

  auto hook = [this](std::string name, std::string table_name,
                     std::string operation, int rowid) {
    if (update_hook_callback != nullptr) {
      invoker->invokeAsync([this, callback = update_hook_callback, table_name,
                            operation = std::move(operation), rowid] {
        auto res = jsi::Object(rt);
        res.setProperty(rt, "table",
                        jsi::String::createFromUtf8(rt, table_name));
        res.setProperty(rt, "operation",
                        jsi::String::createFromUtf8(rt, operation));
        res.setProperty(rt, "rowId", jsi::Value(rowid));

        callback->asObject(rt).asFunction(rt).call(rt, res);
      });
    }

    for (const auto &query_ptr : reactive_queries) {
      auto query = query_ptr.get();
      if (query->discriminators.empty()) {
        continue;
      }

      bool shouldFire = false;

      for (const auto &discriminator : query->discriminators) {
        // Tables don't match then skip
        if (discriminator.table != table_name) {
          continue;
        }

        // If no ids are specified, then we should fire
        if (discriminator.ids.size() == 0) {
          shouldFire = true;
          break;
        }

        // If ids are specified, then we should check if the rowId matches
        for (const auto &discrimator_id : discriminator.ids) {
          if (rowid == discrimator_id) {
            shouldFire = true;
            break;
          }
        }
      }

      if (shouldFire) {
        pending_reactive_queries.insert(query_ptr);
      }
    }
  };

  opsqlite_register_update_hook(db_name, std::move(hook));
  is_update_hook_registered = true;
}
#endif

//    _____                _                   _
//   / ____|              | |                 | |
//  | |     ___  _ __  ___| |_ _ __ _   _  ___| |_ ___  _ __
//  | |    / _ \| '_ \/ __| __| '__| | | |/ __| __/ _ \| '__|
//  | |___| (_) | | | \__ \ |_| |  | |_| | (__| || (_) | |
//   \_____\___/|_| |_|___/\__|_|   \__,_|\___|\__\___/|_|
#ifdef OP_SQLITE_USE_LIBSQL
DBHostObject::DBHostObject(jsi::Runtime &rt, std::string &url,
                           std::string &auth_token,
                           std::shared_ptr<react::CallInvoker> invoker,
                           std::shared_ptr<ThreadPool> thread_pool)
    : db_name(url), invoker(std::move(invoker)),
      thread_pool(std::move(thread_pool)), rt(rt) {
  BridgeResult result = opsqlite_libsql_open_remote(url, auth_token);

  if (result.type == SQLiteError) {
    throw std::runtime_error(result.message);
  }

  create_jsi_functions();
}

DBHostObject::DBHostObject(jsi::Runtime &rt,
                           std::shared_ptr<react::CallInvoker> invoker,
                           std::shared_ptr<ThreadPool> thread_pool,
                           std::string &db_name, std::string &path,
                           std::string &url, std::string &auth_token,
                           int sync_interval)
    : db_name(db_name), invoker(std::move(invoker)),
      thread_pool(std::move(thread_pool)), rt(rt) {
  BridgeResult result =
      opsqlite_libsql_open_sync(db_name, path, url, auth_token, sync_interval);

  if (result.type == SQLiteError) {
    throw std::runtime_error(result.message);
  }

  create_jsi_functions();
}

#endif

DBHostObject::DBHostObject(jsi::Runtime &rt, std::string &base_path,
                           std::shared_ptr<react::CallInvoker> invoker,
                           std::shared_ptr<ThreadPool> thread_pool,
                           std::string &db_name, std::string &path,
                           std::string &crsqlite_path,
                           std::string &sqlite_vec_path,
                           std::string &encryption_key)
    : base_path(base_path), invoker(std::move(invoker)),
      thread_pool(std::move(thread_pool)), db_name(db_name), rt(rt) {

#ifdef OP_SQLITE_USE_SQLCIPHER
  BridgeResult result = opsqlite_open(db_name, path, crsqlite_path,
                                      sqlite_vec_path, encryption_key);
#elif OP_SQLITE_USE_LIBSQL
  BridgeResult result = opsqlite_libsql_open(db_name, path, crsqlite_path);
#else
  db = opsqlite_open(db_name, path, crsqlite_path, sqlite_vec_path);
#endif
  create_jsi_functions();
};

void DBHostObject::create_jsi_functions() {
  auto attach = HOSTFN("attach") {
    if (count < 3) {
      throw jsi::JSError(rt,
                         "[op-sqlite][attach] Incorrect number of arguments");
    }
    if (!args[0].isString() || !args[1].isString() || !args[2].isString()) {
      throw jsi::JSError(
          rt, "dbName, databaseToAttach and alias must be a strings");
      return {};
    }

    std::string tempDocPath = std::string(base_path);
    if (count > 3 && !args[3].isUndefined() && !args[3].isNull()) {
      if (!args[3].isString()) {
        throw std::runtime_error(
            "[op-sqlite][attach] database location must be a string");
      }

      tempDocPath = tempDocPath + "/" + args[3].asString(rt).utf8(rt);
    }

    std::string dbName = args[0].asString(rt).utf8(rt);
    std::string databaseToAttach = args[1].asString(rt).utf8(rt);
    std::string alias = args[2].asString(rt).utf8(rt);
#ifdef OP_SQLITE_USE_LIBSQL
    BridgeResult result =
        opsqlite_libsql_attach(dbName, tempDocPath, databaseToAttach, alias);
#else
    BridgeResult result =
        opsqlite_attach(db, dbName, tempDocPath, databaseToAttach, alias);
#endif
    if (result.type == SQLiteError) {
      throw std::runtime_error(result.message);
    }

    return {};
  });

  auto detach = HOSTFN("detach") {
    if (count < 2) {
      throw std::runtime_error(
          "[op-sqlite][detach] Incorrect number of arguments");
    }
    if (!args[0].isString() || !args[1].isString()) {
      throw std::runtime_error(
          "dbName, databaseToAttach and alias must be a strings");
      return {};
    }

    std::string dbName = args[0].asString(rt).utf8(rt);
    std::string alias = args[1].asString(rt).utf8(rt);
#ifdef OP_SQLITE_USE_LIBSQL
    BridgeResult result = opsqlite_libsql_detach(dbName, alias);
#else
    BridgeResult result = opsqlite_detach(db, dbName, alias);
#endif

    if (result.type == SQLiteError) {
      throw jsi::JSError(rt, result.message.c_str());
    }

    return {};
  });

  auto close = HOSTFN("close") {
#ifdef OP_SQLITE_USE_LIBSQL
    BridgeResult result = opsqlite_libsql_close(db_name);
#else
    opsqlite_close(db);
#endif

    return {};
  });

  auto remove = HOSTFN("delete") {
    std::string path = std::string(base_path);

    if (count == 1 && !args[0].isUndefined() && !args[0].isNull()) {
      if (!args[1].isString()) {
        throw std::runtime_error(
            "[op-sqlite][open] database location must be a string");
      }

      std::string location = args[1].asString(rt).utf8(rt);

      if (!location.empty()) {
        if (location == ":memory:") {
          path = ":memory:";
        } else if (location.rfind('/', 0) == 0) {
          path = location;
        } else {
          path = path + "/" + location;
        }
      }
    }

#ifdef OP_SQLITE_USE_LIBSQL
    BridgeResult result = opsqlite_libsql_remove(db_name, path);
#else
    opsqlite_remove(db, db_name, path);
#endif

    return {};
  });

  auto execute_raw = HOSTFN("executeRaw") {
    const std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params;

    if (count == 2) {
      params = to_variant_vec(rt, args[1]);
    }

    auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
    auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor") {
      auto resolve = std::make_shared<jsi::Value>(rt, args[0]);
      auto reject = std::make_shared<jsi::Value>(rt, args[1]);

      auto task = [&rt, this, query, params = std::move(params), resolve,
                   reject]() {
        try {
          std::vector<std::vector<JSVariant>> results;

#ifdef OP_SQLITE_USE_LIBSQL
          auto status =
              opsqlite_libsql_execute_raw(db_name, query, &params, &results);
#else
          auto status = opsqlite_execute_raw(db, query, &params, &results);
#endif

          if (invalidated) {
            return;
          }

          invoker->invokeAsync([&rt, results = std::move(results),
                                status = std::move(status), resolve, reject] {
            if (status.type == SQLiteOk) {
              auto jsiResult = create_raw_result(rt, status, &results);
              resolve->asObject(rt).asFunction(rt).call(rt,
                                                        std::move(jsiResult));
            } else {
              auto errorCtr = rt.global().getPropertyAsFunction(rt, "Error");
              auto error = errorCtr.callAsConstructor(
                  rt, jsi::String::createFromUtf8(rt, status.message));
              reject->asObject(rt).asFunction(rt).call(rt, error);
            }
          });

        } catch (std::exception &exc) {
          auto what = exc.what();
          invoker->invokeAsync([&rt, what = std::move(what), reject] {
            auto errorCtr = rt.global().getPropertyAsFunction(rt, "Error");
            auto error = errorCtr.callAsConstructor(
                rt, jsi::String::createFromAscii(rt, what));
            reject->asObject(rt).asFunction(rt).call(rt, error);
          });
        }
      };

      thread_pool->queueWork(task);

      return {};
     }));

    return promise;
  });

  auto execute_sync = HOSTFN("executeSync") {

    std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params;

    if (count == 2) {
      params = to_variant_vec(rt, args[1]);
    }
#ifdef OP_SQLITE_USE_LIBSQL
    auto status = opsqlite_libsql_execute(db_name, query, &params);
#else
    auto status = opsqlite_execute(db, query, &params);
#endif

    if (status.type != SQLiteOk) {
      throw std::runtime_error(status.message);
    }
    return create_js_rows(rt, status);
  });

  auto execute = HOSTFN("execute") {
    std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params;

    if (count == 2) {
      params = to_variant_vec(rt, args[1]);
    }

    auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
    auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor") {
      auto resolve = std::make_shared<jsi::Value>(rt, args[0]);
      auto reject = std::make_shared<jsi::Value>(rt, args[1]);

      auto task = [&rt, this, query = std::move(query),
                   params = std::move(params), resolve, reject]() {
        try {

#ifdef OP_SQLITE_USE_LIBSQL
          auto status = opsqlite_libsql_execute(db_name, query, &params);
#else
          auto status = opsqlite_execute(db, query, &params);
#endif

          if (invalidated) {
            return;
          }

          invoker->invokeAsync([&rt, status = std::move(status), resolve,
                                reject] {
            if (status.type == SQLiteOk) {
              auto jsiResult = create_js_rows(rt, status);
              resolve->asObject(rt).asFunction(rt).call(rt,
                                                        std::move(jsiResult));
            } else {
              auto errorCtr = rt.global().getPropertyAsFunction(rt, "Error");
              auto error = errorCtr.callAsConstructor(
                  rt, jsi::String::createFromUtf8(rt, status.message));
              reject->asObject(rt).asFunction(rt).call(rt, error);
            }
          });

        } catch (std::exception &exc) {
          auto what = exc.what();
          invoker->invokeAsync([&rt, what = std::move(what), reject] {
            auto errorCtr = rt.global().getPropertyAsFunction(rt, "Error");

            auto error = errorCtr.callAsConstructor(
                rt, jsi::String::createFromAscii(rt, what));
            reject->asObject(rt).asFunction(rt).call(rt, error);
          });
        }
      };

      thread_pool->queueWork(task);

      return {};
        }));

    return promise;
  });

  auto execute_with_host_objects = HOSTFN("executeWithHostObjects") {
    const std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params;

    if (count == 2) {
      const jsi::Value &originalParams = args[1];
      params = to_variant_vec(rt, originalParams);
    }

    auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
    auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor") {
      auto resolve = std::make_shared<jsi::Value>(rt, args[0]);
      auto reject = std::make_shared<jsi::Value>(rt, args[1]);

      auto task = [&rt, this, query, params = std::move(params), resolve,
                   reject, invoker = this->invoker]() {
        try {
          std::vector<DumbHostObject> results;
          std::shared_ptr<std::vector<SmartHostObject>> metadata =
              std::make_shared<std::vector<SmartHostObject>>();
#ifdef OP_SQLITE_USE_LIBSQL
          auto status = opsqlite_libsql_execute_with_host_objects(
              db_name, query, &params, &results, metadata);
#else
          auto status = opsqlite_execute_host_objects(db, query, &params,
                                                      &results, metadata);
#endif

          if (invalidated) {
            return;
          }

          invoker->invokeAsync(
              [&rt,
               results = std::make_shared<std::vector<DumbHostObject>>(results),
               metadata, status = std::move(status), resolve, reject] {
                if (status.type == SQLiteOk) {
                  auto jsiResult =
                      create_result(rt, status, results.get(), metadata);
                  resolve->asObject(rt).asFunction(rt).call(
                      rt, std::move(jsiResult));
                } else {
                  auto errorCtr =
                      rt.global().getPropertyAsFunction(rt, "Error");
                  auto error = errorCtr.callAsConstructor(
                      rt, jsi::String::createFromUtf8(rt, status.message));
                  reject->asObject(rt).asFunction(rt).call(rt, error);
                }
              });

        } catch (std::exception &exc) {
          auto what = exc.what();
          invoker->invokeAsync([&rt, what = std::move(what), reject] {
            auto errorCtr = rt.global().getPropertyAsFunction(rt, "Error");
            auto error = errorCtr.callAsConstructor(
                rt, jsi::String::createFromAscii(rt, what));
            reject->asObject(rt).asFunction(rt).call(rt, error);
          });
        }
      };

      thread_pool->queueWork(task);

      return {};
      }));

    return promise;
  });

  auto execute_batch = HOSTFN("executeBatch") {
    if (sizeof(args) < 1) {
      throw std::runtime_error(
          "[op-sqlite][executeAsyncBatch] Incorrect parameter count");
      return {};
    }

    const jsi::Value &params = args[0];

    if (params.isNull() || params.isUndefined()) {
      throw std::runtime_error(
          "[op-sqlite][executeAsyncBatch] - An array of SQL "
          "commands or parameters is needed");
      return {};
    }

    const jsi::Array &batchParams = params.asObject(rt).asArray(rt);

    std::vector<BatchArguments> commands;
    to_batch_arguments(rt, batchParams, &commands);

    auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
     auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor") {
      auto resolve = std::make_shared<jsi::Value>(rt, args[0]);
      auto reject = std::make_shared<jsi::Value>(rt, args[1]);

      auto task = [&rt, this,
                   commands =
                       std::make_shared<std::vector<BatchArguments>>(commands),
                   resolve, reject]() {
        try {
#ifdef OP_SQLITE_USE_LIBSQL
          auto batchResult =
              opsqlite_libsql_execute_batch(db_name, commands.get());
#else
          auto batchResult = opsqlite_execute_batch(db, commands.get());
#endif

          if (invalidated) {
            return;
          }

          invoker->invokeAsync([&rt, batchResult = std::move(batchResult),
                                resolve, reject] {
            if (batchResult.type == SQLiteOk) {
              auto res = jsi::Object(rt);
              res.setProperty(rt, "rowsAffected",
                              jsi::Value(batchResult.affectedRows));
              resolve->asObject(rt).asFunction(rt).call(rt, std::move(res));
            } else {
              auto errorCtr = rt.global().getPropertyAsFunction(rt, "Error");
              auto error = errorCtr.callAsConstructor(
                  rt, jsi::String::createFromUtf8(rt, batchResult.message));
              reject->asObject(rt).asFunction(rt).call(rt, error);
            }
          });
        } catch (std::exception &exc) {
          auto what = exc.what();
          invoker->invokeAsync([&rt, what = std::move(what), reject] {
            auto errorCtr = rt.global().getPropertyAsFunction(rt, "Error");
            auto error = errorCtr.callAsConstructor(
                rt, jsi::String::createFromAscii(rt, what));
            reject->asObject(rt).asFunction(rt).call(rt, error);
          });
        }
      };
      thread_pool->queueWork(task);

      return {};
            }));

     return promise;
  });

#ifdef OP_SQLITE_USE_LIBSQL
  auto sync = HOSTFN("sync") {
    BridgeResult result = opsqlite_libsql_sync(db_name);
    if (result.type == SQLiteError) {
      throw std::runtime_error(result.message);
    }
    return {};
  });
#else
  auto load_file = HOSTFN("loadFile") {
    if (sizeof(args) < 1) {
      throw std::runtime_error(
          "[op-sqlite][loadFile] Incorrect parameter count");
      return {};
    }

    const std::string sqlFileName = args[0].asString(rt).utf8(rt);

    auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
        auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor")
        {
      auto resolve = std::make_shared<jsi::Value>(rt, args[0]);
      auto reject = std::make_shared<jsi::Value>(rt, args[1]);

      auto task = [&rt, this, sqlFileName, resolve, reject]() {
        try {
          const auto result = import_sql_file(db, sqlFileName);

          invoker->invokeAsync([&rt, result = std::move(result), resolve,
                                reject] {
            if (result.type == SQLiteOk) {
              auto res = jsi::Object(rt);
              res.setProperty(rt, "rowsAffected",
                              jsi::Value(result.affectedRows));
              res.setProperty(rt, "commands", jsi::Value(result.commands));
              resolve->asObject(rt).asFunction(rt).call(rt, std::move(res));
            } else {
              auto errorCtr = rt.global().getPropertyAsFunction(rt, "Error");
              auto error = errorCtr.callAsConstructor(
                  rt, jsi::String::createFromUtf8(rt, result.message));
              reject->asObject(rt).asFunction(rt).call(rt, error);
            }
          });
        } catch (std::exception &exc) {
          auto what = exc.what();
          invoker->invokeAsync([&rt, what = std::move(what), reject] {
            auto errorCtr = rt.global().getPropertyAsFunction(rt, "Error");
            auto error = errorCtr.callAsConstructor(
                rt, jsi::String::createFromAscii(rt, what));
            reject->asObject(rt).asFunction(rt).call(rt, error);
          });
        }
      };
      thread_pool->queueWork(task);
      return {};
               }));

        return promise;
  });

  auto update_hook = HOSTFN("updateHook") {
    auto callback = std::make_shared<jsi::Value>(rt, args[0]);

    if (callback->isUndefined() || callback->isNull()) {
      update_hook_callback = nullptr;
    } else {
      update_hook_callback = callback;
    }
    auto_register_update_hook();
    return {};
  });

  auto commit_hook = HOSTFN("commitHook") {
    if (sizeof(args) < 1) {
      throw std::runtime_error("[op-sqlite][commitHook] callback needed");
      return {};
    }

    auto callback = std::make_shared<jsi::Value>(rt, args[0]);
    if (callback->isUndefined() || callback->isNull()) {
      opsqlite_deregister_commit_hook(db_name);
      return {};
    }
    commit_hook_callback = callback;

    auto hook = [&rt, this, callback](std::string dbName) {
      invoker->invokeAsync(
          [&rt, callback] { callback->asObject(rt).asFunction(rt).call(rt); });
    };

    opsqlite_register_commit_hook(db_name, std::move(hook));

    return {};
  });

  auto rollback_hook = HOSTFN("rollbackHook") {
    if (sizeof(args) < 1) {
      throw std::runtime_error("[op-sqlite][rollbackHook] callback needed");
      return {};
    }

    auto callback = std::make_shared<jsi::Value>(rt, args[0]);

    if (callback->isUndefined() || callback->isNull()) {
      opsqlite_deregister_rollback_hook(db_name);
      return {};
    }
    rollback_hook_callback = callback;

    auto hook = [&rt, this, callback](std::string db_name) {
      invoker->invokeAsync(
          [&rt, callback] { callback->asObject(rt).asFunction(rt).call(rt); });
    };

    opsqlite_register_rollback_hook(db_name, std::move(hook));
    return {};
  });

  auto load_extension = HOSTFN("loadExtension") {
    auto path = args[0].asString(rt).utf8(rt);
    std::string entry_point = "";
    if (count > 1 && args[1].isString()) {
      entry_point = args[1].asString(rt).utf8(rt);
    }

    opsqlite_load_extension(db, path, entry_point);
    return {};
  });

  auto reactive_execute = HOSTFN("reactiveExecute") {
    auto query = args[0].asObject(rt);
    // if (!query.hasProperty(rt, "query") || !query.hasProperty(rt, "args")
    // ||
    //     !query.hasProperty(rt, "tables") || !query.hasProperty(rt,
    //     "rowIds")
    //     || !query.hasProperty(rt, "callback")) {
    //   throw std::runtime_error(
    //       "[op-sqlite][reactiveExecute] Query object must have query, args,
    //       " "tables, rowIds and callback properties");
    // }

    const std::string query_str =
        query.getProperty(rt, "query").asString(rt).utf8(rt);
    auto js_args = query.getProperty(rt, "arguments");
    auto js_discriminators =
        query.getProperty(rt, "fireOn").asObject(rt).asArray(rt);
    auto variant_args = to_variant_vec(rt, js_args);

    sqlite3_stmt *stmt = opsqlite_prepare_statement(db, query_str);
    opsqlite_bind_statement(stmt, &variant_args);

    auto callback =
        std::make_shared<jsi::Value>(query.getProperty(rt, "callback"));

    // std::vector<JSVariant> query_args = to_variant_vec(rt, argsArray);
    // std::vector<std::string> tables = to_string_vec(rt, tablesArray);
    // std::vector<int> rowIds;
    // if (query.hasProperty(rt, "rowIds")) {
    //   auto rowIdsArray = query.getProperty(rt, "rowIds");
    //   rowIds = to_int_vec(rt, rowIdsArray);
    // }

    std::vector<TableRowDiscriminator> discriminators;

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

    std::shared_ptr<ReactiveQuery> reactiveQuery =
        std::make_shared<ReactiveQuery>(
            ReactiveQuery{stmt, discriminators, callback});

    reactive_queries.push_back(reactiveQuery);

    auto_register_update_hook();

    auto unsubscribe = HOSTFN("unsubscribe") {
      auto it = std::find(reactive_queries.begin(), reactive_queries.end(),
                          reactiveQuery);
      if (it != reactive_queries.end()) {
        reactive_queries.erase(it);
      }
      auto_register_update_hook();
      return {};
    });

    return unsubscribe;
  });

#endif

  auto prepare_statement = HOSTFN("prepareStatement") {
    auto query = args[0].asString(rt).utf8(rt);
#ifdef OP_SQLITE_USE_LIBSQL
    libsql_stmt_t statement = opsqlite_libsql_prepare_statement(db_name, query);
#else
    sqlite3_stmt *statement = opsqlite_prepare_statement(db, query);
#endif
    auto preparedStatementHostObject =
        std::make_shared<PreparedStatementHostObject>(db, db_name, statement,
                                                      invoker, thread_pool);

    return jsi::Object::createFromHostObject(rt, preparedStatementHostObject);
  });

  auto get_db_path = HOSTFN("getDbPath") {
    std::string path = std::string(base_path);
    if (count == 1 && !args[0].isUndefined() && !args[0].isNull()) {
      if (!args[0].isString()) {
        throw std::runtime_error(
            "[op-sqlite][open] database location must be a string");
      }

      std::string lastPath = args[0].asString(rt).utf8(rt);

      if (lastPath == ":memory:") {
        path = ":memory:";
      } else if (lastPath.rfind("/", 0) == 0) {
        path = lastPath;
      } else {
        path = path + "/" + lastPath;
      }
    }

    auto result = opsqlite_get_db_path(db_name, path);
    return jsi::String::createFromUtf8(rt, result);
  });

  auto flush_pending_reactive_queries_js =
      HOSTFN("flushPendingReactiveQueries") {
    auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
          auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor") {
      auto resolve = std::make_shared<jsi::Value>(rt, args[0]);

      auto task = [&rt, this, resolve]() {
        flush_pending_reactive_queries(resolve);
      };

      thread_pool->queueWork(task);

      return {};
              }));

          return promise;
  });

  function_map["attach"] = std::move(attach);
  function_map["detach"] = std::move(detach);
  function_map["close"] = std::move(close);
  function_map["execute"] = std::move(execute);
  function_map["executeSync"] = std::move(execute_sync);
  function_map["executeRaw"] = std::move(execute_raw);
  function_map["executeWithHostObjects"] = std::move(execute_with_host_objects);
  function_map["delete"] = std::move(remove);
  function_map["executeBatch"] = std::move(execute_batch);
  function_map["prepareStatement"] = std::move(prepare_statement);
  function_map["getDbPath"] = std::move(get_db_path);
  function_map["flushPendingReactiveQueries"] =
      std::move(flush_pending_reactive_queries_js);
#ifdef OP_SQLITE_USE_LIBSQL
  function_map["sync"] = std::move(sync);
#else
  function_map["loadFile"] = std::move(load_file);
  function_map["updateHook"] = std::move(update_hook);
  function_map["commitHook"] = std::move(commit_hook);
  function_map["rollbackHook"] = std::move(rollback_hook);
  function_map["loadExtension"] = std::move(load_extension);
  function_map["reactiveExecute"] = std::move(reactive_execute);
#endif
}

std::vector<jsi::PropNameID> DBHostObject::getPropertyNames(jsi::Runtime &rt) {
  std::vector<jsi::PropNameID> keys;

  return keys;
}

jsi::Value DBHostObject::get(jsi::Runtime &rt,
                             const jsi::PropNameID &propNameID) {
  auto name = propNameID.utf8(rt);
  if (function_map.count(name) != 1) {
    return HOSTFN(name.c_str()) {
      throw std::runtime_error(
          "[op-sqlite] Function " + name +
          " not implemented for current backend (libsql or sqlcipher)");
    });
  }

  return jsi::Value(rt, function_map[name]);
}

void DBHostObject::set(jsi::Runtime &rt, const jsi::PropNameID &name,
                       const jsi::Value &value) {
  throw std::runtime_error("You cannot write to this object!");
}

void DBHostObject::invalidate() { invalidated = true; }

DBHostObject::~DBHostObject() { invalidated = true; }

} // namespace opsqlite
