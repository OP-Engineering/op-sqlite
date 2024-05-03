#include "bindings.h"
#include "DumbHostObject.h"
#include "PreparedStatementHostObject.h"
#include "ThreadPool.h"
#include "bridge.h"
#include "logs.h"
#include "macros.h"
#include "sqlbatchexecutor.h"
#include "utils.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;

std::string basePath;
std::string crsqlitePath;
std::shared_ptr<react::CallInvoker> invoker;
ThreadPool pool;
std::unordered_map<std::string, std::shared_ptr<jsi::Value>> updateHooks =
    std::unordered_map<std::string, std::shared_ptr<jsi::Value>>();
std::unordered_map<std::string, std::shared_ptr<jsi::Value>> commitHooks =
    std::unordered_map<std::string, std::shared_ptr<jsi::Value>>();
std::unordered_map<std::string, std::shared_ptr<jsi::Value>> rollbackHooks =
    std::unordered_map<std::string, std::shared_ptr<jsi::Value>>();

// React native will try to clean the module on JS context invalidation
// (CodePush/Hot Reload) The clearState function is called and we use this flag
// to prevent any ongoing operations from continuing work and can return early
bool invalidated = false;

void clearState() {
  invalidated = true;
  // Will terminate all operations and database connections
  opsqlite_close_all();
  // We then join all the threads before the context gets invalidated
  pool.restartPool();
  updateHooks.clear();
  commitHooks.clear();
  rollbackHooks.clear();
}

void install(jsi::Runtime &rt,
             std::shared_ptr<react::CallInvoker> jsCallInvoker,
             const char *docPath, const char *_crsqlitePath) {

  invalidated = false;
  basePath = std::string(docPath);
  crsqlitePath = std::string(_crsqlitePath);
  invoker = jsCallInvoker;

  auto open = HOSTFN("open", 3) {
    if (count == 0) {
      throw std::runtime_error("[op-sqlite][open] database name is required");
    }

    jsi::Object options = args[0].asObject(rt);
    std::string dbName = options.getProperty(rt, "name").asString(rt).utf8(rt);
    std::string path = std::string(basePath);
    std::string location;
    std::string encryptionKey;

    if (options.hasProperty(rt, "location")) {
      location = options.getProperty(rt, "location").asString(rt).utf8(rt);
    }

    if (options.hasProperty(rt, "encryptionKey")) {
      encryptionKey =
          options.getProperty(rt, "encryptionKey").asString(rt).utf8(rt);
    }

#ifdef OP_SQLITE_USE_SQLCIPHER
    if (encryptionKey.empty()) {
      throw std::runtime_error(
          "[OP SQLite] using SQLCipher encryption key is required");
    }
//      TODO(osp) find a way to display the yellow box from c++
#else
    // if (!encryptionKey.empty()) {
    //   //  RCTLogWarn(@"Your message")
    //   throw std::runtime_error("[OP SQLite] SQLCipher is not enabled, "
    //                            "encryption key is not allowed");
    // }
#endif

    if (!location.empty()) {
      if (location == ":memory:") {
        path = ":memory:";
      } else if (location.rfind("/", 0) == 0) {
        path = location;
      } else {
        path = path + "/" + location;
      }
    }

#ifdef OP_SQLITE_USE_SQLCIPHER
    BridgeResult result =
        opsqlite_open(dbName, path, crsqlitePath, encryptionKey);
#else
    BridgeResult result = opsqlite_open(dbName, path, crsqlitePath);
#endif

    if (result.type == SQLiteError) {
      throw std::runtime_error(result.message);
    }

    return {};
  });

  auto attach = HOSTFN("attach", 4) {
    if (count < 3) {
      throw jsi::JSError(rt,
                         "[op-sqlite][attach] Incorrect number of arguments");
    }
    if (!args[0].isString() || !args[1].isString() || !args[2].isString()) {
      throw jsi::JSError(
          rt, "dbName, databaseToAttach and alias must be a strings");
      return {};
    }

    std::string tempDocPath = std::string(basePath);
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
    BridgeResult result =
        opsqlite_attach(dbName, tempDocPath, databaseToAttach, alias);

    if (result.type == SQLiteError) {
      throw std::runtime_error(result.message);
    }

    return {};
  });

  auto detach = HOSTFN("detach", 2) {
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
    BridgeResult result = opsqlite_detach(dbName, alias);

    if (result.type == SQLiteError) {
      throw jsi::JSError(rt, result.message.c_str());
    }

    return {};
  });

  auto close = HOSTFN("close", 1) {
    if (count == 0) {
      throw std::runtime_error("[op-sqlite][close] database name is required");
    }

    if (!args[0].isString()) {
      throw std::runtime_error(
          "[op-sqlite][close] database name must be a string");
    }

    std::string dbName = args[0].asString(rt).utf8(rt);

    BridgeResult result = opsqlite_close(dbName);

    if (result.type == SQLiteError) {
      throw jsi::JSError(rt, result.message.c_str());
    }

    return {};
  });

  auto remove = HOSTFN("delete", 2) {
    if (count == 0) {
      throw std::runtime_error("[op-sqlite][open] database name is required");
    }

    if (!args[0].isString()) {
      throw std::runtime_error(
          "[op-sqlite][open] database name must be a string");
    }

    std::string dbName = args[0].asString(rt).utf8(rt);

    std::string path = std::string(basePath);

    if (count > 1 && !args[1].isUndefined() && !args[1].isNull()) {
      if (!args[1].isString()) {
        throw std::runtime_error(
            "[op-sqlite][open] database location must be a string");
      }

      std::string location = args[1].asString(rt).utf8(rt);

      if (!location.empty()) {
        if (location == ":memory:") {
          path = ":memory:";
        } else if (location.rfind("/", 0) == 0) {
          path = location;
        } else {
          path = path + "/" + location;
        }
      }
    }

    BridgeResult result = opsqlite_remove(dbName, path);

    if (result.type == SQLiteError) {
      throw std::runtime_error(result.message);
    }

    return {};
  });

  auto execute = HOSTFN("execute", 3) {
    const std::string dbName = args[0].asString(rt).utf8(rt);
    const std::string query = args[1].asString(rt).utf8(rt);
    std::vector<JSVariant> params;

    if (count == 3) {
      const jsi::Value &originalParams = args[2];
      params = toVariantVec(rt, originalParams);
    }

    std::vector<DumbHostObject> results;
    std::shared_ptr<std::vector<SmartHostObject>> metadata =
        std::make_shared<std::vector<SmartHostObject>>();

    auto status = opsqlite_execute(dbName, query, &params, &results, metadata);

    if (status.type == SQLiteError) {
      throw std::runtime_error(status.message);
    }

    auto jsiResult = createResult(rt, status, &results, metadata);
    return jsiResult;
  });

  auto execute_raw_async = HOSTFN("executeRawAsync", 3) {
    if (count < 3) {
      throw std::runtime_error(
          "[op-sqlite][executeAsync] Incorrect arguments for executeAsync");
    }

    const std::string dbName = args[0].asString(rt).utf8(rt);
    const std::string query = args[1].asString(rt).utf8(rt);
    const jsi::Value &originalParams = args[2];

    std::vector<JSVariant> params = toVariantVec(rt, originalParams);

    auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");

        auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor", 2) {
      auto resolve = std::make_shared<jsi::Value>(rt, args[0]);
      auto reject = std::make_shared<jsi::Value>(rt, args[1]);

      auto task = [&rt, dbName, query, params = std::move(params), resolve,
                   reject]() {
        try {
          std::vector<std::vector<JSVariant>> results;

          auto status = opsqlite_execute_raw(dbName, query, &params, &results);

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
          invoker->invokeAsync([&rt, exc = std::move(exc), reject] {
            auto errorCtr = rt.global().getPropertyAsFunction(rt, "Error");
            auto error = errorCtr.callAsConstructor(
                rt, jsi::String::createFromAscii(rt, exc.what()));
            reject->asObject(rt).asFunction(rt).call(rt, error);
          });
        }
      };

      pool.queueWork(task);

      return {};
        }));

        return promise;
  });

  auto execute_async = HOSTFN("executeAsync", 3) {
    if (count < 3) {
      throw std::runtime_error(
          "[op-sqlite][executeAsync] Incorrect arguments for executeAsync");
    }

    const std::string dbName = args[0].asString(rt).utf8(rt);
    const std::string query = args[1].asString(rt).utf8(rt);
    const jsi::Value &originalParams = args[2];

    std::vector<JSVariant> params = toVariantVec(rt, originalParams);

    auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");

    auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor", 2) {
      auto resolve = std::make_shared<jsi::Value>(rt, args[0]);
      auto reject = std::make_shared<jsi::Value>(rt, args[1]);

      auto task = [&rt, dbName, query, params = std::move(params), resolve,
                   reject]() {
        try {
          std::vector<DumbHostObject> results;
          std::shared_ptr<std::vector<SmartHostObject>> metadata =
              std::make_shared<std::vector<SmartHostObject>>();

          auto status =
              opsqlite_execute(dbName, query, &params, &results, metadata);

          if (invalidated) {
            return;
          }

          invoker->invokeAsync(
              [&rt,
               results = std::make_shared<std::vector<DumbHostObject>>(results),
               metadata, status = std::move(status), resolve, reject] {
                if (status.type == SQLiteOk) {
                  auto jsiResult =
                      createResult(rt, status, results.get(), metadata);
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
          invoker->invokeAsync([&rt, exc = std::move(exc), reject] {
            auto errorCtr = rt.global().getPropertyAsFunction(rt, "Error");
            auto error = errorCtr.callAsConstructor(
                rt, jsi::String::createFromAscii(rt, exc.what()));
            reject->asObject(rt).asFunction(rt).call(rt, error);
          });
        }
      };

      pool.queueWork(task);

      return {};
    }));

    return promise;
  });

  auto execute_batch = HOSTFN("executeBatch", 2) {
    if (sizeof(args) < 2) {
      throw std::runtime_error(
          "[op-sqlite][executeBatch] - Incorrect parameter count");
    }

    const jsi::Value &params = args[1];
    if (params.isNull() || params.isUndefined()) {
      throw std::runtime_error("[op-sqlite][executeBatch] - An array of SQL "
                               "commands or parameters is needed");
    }
    const std::string dbName = args[0].asString(rt).utf8(rt);
    const jsi::Array &batchParams = params.asObject(rt).asArray(rt);
    std::vector<BatchArguments> commands;
    toBatchArguments(rt, batchParams, &commands);

    auto batchResult = sqliteExecuteBatch(dbName, &commands);
    if (batchResult.type == SQLiteOk) {
      auto res = jsi::Object(rt);
      res.setProperty(rt, "rowsAffected", jsi::Value(batchResult.affectedRows));
      return std::move(res);
    } else {
      throw std::runtime_error(batchResult.message);
    }
  });

  auto execute_batch_async = HOSTFN("executeBatchAsync", 2) {
    if (sizeof(args) < 2) {
      throw std::runtime_error(
          "[op-sqlite][executeAsyncBatch] Incorrect parameter count");
      return {};
    }

    const jsi::Value &params = args[1];

    if (params.isNull() || params.isUndefined()) {
      throw std::runtime_error(
          "[op-sqlite][executeAsyncBatch] - An array of SQL "
          "commands or parameters is needed");
      return {};
    }

    const std::string dbName = args[0].asString(rt).utf8(rt);
    const jsi::Array &batchParams = params.asObject(rt).asArray(rt);

    std::vector<BatchArguments> commands;
    toBatchArguments(rt, batchParams, &commands);

    auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
        auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor", 2) {
      auto resolve = std::make_shared<jsi::Value>(rt, args[0]);
      auto reject = std::make_shared<jsi::Value>(rt, args[1]);

      auto task = [&rt, dbName,
                   commands =
                       std::make_shared<std::vector<BatchArguments>>(commands),
                   resolve, reject]() {
        try {
          auto batchResult = sqliteExecuteBatch(dbName, commands.get());
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
          invoker->invokeAsync(
              [&rt, reject, &exc] { throw jsi::JSError(rt, exc.what()); });
        }
      };
      pool.queueWork(task);

      return {};
        }));

        return promise;
  });

  auto load_file = HOSTFN("loadFile", 2) {
    if (sizeof(args) < 2) {
      throw std::runtime_error(
          "[op-sqlite][loadFile] Incorrect parameter count");
      return {};
    }

    const std::string dbName = args[0].asString(rt).utf8(rt);
    const std::string sqlFileName = args[1].asString(rt).utf8(rt);

    auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
        auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor", 2) {
      auto resolve = std::make_shared<jsi::Value>(rt, args[0]);
      auto reject = std::make_shared<jsi::Value>(rt, args[1]);

      auto task = [&rt, dbName, sqlFileName, resolve, reject]() {
        try {
          const auto importResult = importSQLFile(dbName, sqlFileName);

          invoker->invokeAsync([&rt, result = std::move(importResult), resolve,
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
          invoker->invokeAsync(
              [&rt, err = exc.what(), reject] { throw jsi::JSError(rt, err); });
        }
      };
      pool.queueWork(task);
      return {};
        }));

        return promise;
  });

  auto update_hook = HOSTFN("updateHook", 2) {
    if (sizeof(args) < 2) {
      throw std::runtime_error("[op-sqlite][updateHook] Incorrect parameters: "
                               "dbName and callback needed");
      return {};
    }

    auto dbName = args[0].asString(rt).utf8(rt);
    auto callback = std::make_shared<jsi::Value>(rt, args[1]);

    if (callback->isUndefined() || callback->isNull()) {
      opsqlite_deregister_update_hook(dbName);
      return {};
    }

    updateHooks[dbName] = callback;

    auto hook = [&rt, callback](std::string dbName, std::string tableName,
                                std::string operation, int rowId) {
      std::vector<JSVariant> params;
      std::vector<DumbHostObject> results;
      std::shared_ptr<std::vector<SmartHostObject>> metadata =
          std::make_shared<std::vector<SmartHostObject>>();

      if (operation != "DELETE") {
        std::string query = "SELECT * FROM " + tableName +
                            " where rowid = " + std::to_string(rowId) + ";";
        opsqlite_execute(dbName, query, &params, &results, metadata);
      }

      invoker->invokeAsync(
          [&rt,
           results = std::make_shared<std::vector<DumbHostObject>>(results),
           callback, tableName = std::move(tableName),
           operation = std::move(operation), &rowId] {
            auto res = jsi::Object(rt);
            res.setProperty(rt, "table",
                            jsi::String::createFromUtf8(rt, tableName));
            res.setProperty(rt, "operation",
                            jsi::String::createFromUtf8(rt, operation));
            res.setProperty(rt, "rowId", jsi::Value(rowId));
            if (results->size() != 0) {
              res.setProperty(
                  rt, "row",
                  jsi::Object::createFromHostObject(
                      rt, std::make_shared<DumbHostObject>(results->at(0))));
            }

            callback->asObject(rt).asFunction(rt).call(rt, res);
          });
    };

    opsqlite_register_update_hook(dbName, std::move(hook));

    return {};
  });

  auto commit_hook = HOSTFN("commitHook", 2) {
    if (sizeof(args) < 2) {
      throw std::runtime_error("[op-sqlite][commitHook] Incorrect parameters: "
                               "dbName and callback needed");
      return {};
    }

    auto dbName = args[0].asString(rt).utf8(rt);
    auto callback = std::make_shared<jsi::Value>(rt, args[1]);
    if (callback->isUndefined() || callback->isNull()) {
      opsqlite_deregister_commit_hook(dbName);
      return {};
    }
    commitHooks[dbName] = callback;

    auto hook = [&rt, callback](std::string dbName) {
      invoker->invokeAsync(
          [&rt, callback] { callback->asObject(rt).asFunction(rt).call(rt); });
    };

    opsqlite_register_commit_hook(dbName, std::move(hook));

    return {};
  });

  auto rollback_hook = HOSTFN("rollbackHook", 2) {
    if (sizeof(args) < 2) {
      throw std::runtime_error(
          "[op-sqlite][rollbackHook] Incorrect parameters: "
          "dbName and callback needed");
      return {};
    }

    auto dbName = args[0].asString(rt).utf8(rt);
    auto callback = std::make_shared<jsi::Value>(rt, args[1]);

    if (callback->isUndefined() || callback->isNull()) {
      opsqlite_deregister_rollback_hook(dbName);
      return {};
    }
    rollbackHooks[dbName] = callback;

    auto hook = [&rt, callback](std::string dbName) {
      invoker->invokeAsync(
          [&rt, callback] { callback->asObject(rt).asFunction(rt).call(rt); });
    };

    opsqlite_register_rollback_hook(dbName, std::move(hook));
    return {};
  });

  auto prepare_statement = HOSTFN("prepareStatement", 1) {
    auto dbName = args[0].asString(rt).utf8(rt);
    auto query = args[1].asString(rt).utf8(rt);

    sqlite3_stmt *statement = opsqlite_prepare_statement(dbName, query);

    auto preparedStatementHostObject =
        std::make_shared<PreparedStatementHostObject>(dbName, statement);

    return jsi::Object::createFromHostObject(rt, preparedStatementHostObject);
  });

  auto load_extension = HOSTFN("loadExtension", 2) {
    auto db_name = args[0].asString(rt).utf8(rt);
    auto path = args[1].asString(rt).utf8(rt);
    std::string entryPoint = "";
    if (count > 2 && args[2].isString()) {
      entryPoint = args[2].asString(rt).utf8(rt);
    }

    auto result = opsqlite_load_extension(db_name, path, entryPoint);
    if (result.type == SQLiteError) {
      throw std::runtime_error(result.message);
    }
    return {};
  });

  auto get_db_path = HOSTFN("getDbPath", 2) {
    std::string db_name = args[0].asString(rt).utf8(rt);
    std::string path = std::string(basePath);
    if (count > 1 && !args[1].isUndefined() && !args[1].isNull()) {
      if (!args[1].isString()) {
        throw std::runtime_error(
            "[op-sqlite][open] database location must be a string");
      }

      std::string lastPath = args[1].asString(rt).utf8(rt);

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

  auto is_sqlcipher = HOSTFN("isSQLCipher", 0) {
#ifdef OP_SQLITE_USE_SQLCIPHER
    return true;
#else
    return false;
#endif
  });

  jsi::Object module = jsi::Object(rt);

  module.setProperty(rt, "open", std::move(open));
  module.setProperty(rt, "close", std::move(close));
  module.setProperty(rt, "attach", std::move(attach));
  module.setProperty(rt, "detach", std::move(detach));
  module.setProperty(rt, "delete", std::move(remove));
  module.setProperty(rt, "execute", std::move(execute));
  module.setProperty(rt, "executeAsync", std::move(execute_async));
  module.setProperty(rt, "executeBatch", std::move(execute_batch));
  module.setProperty(rt, "executeBatchAsync", std::move(execute_batch_async));
  module.setProperty(rt, "loadFile", std::move(load_file));
  module.setProperty(rt, "updateHook", std::move(update_hook));
  module.setProperty(rt, "commitHook", std::move(commit_hook));
  module.setProperty(rt, "rollbackHook", std::move(rollback_hook));
  module.setProperty(rt, "prepareStatement", std::move(prepare_statement));
  module.setProperty(rt, "loadExtension", std::move(load_extension));
  module.setProperty(rt, "executeRawAsync", std::move(execute_raw_async));
  module.setProperty(rt, "getDbPath", std::move(get_db_path));
  module.setProperty(rt, "isSQLCipher", std::move(is_sqlcipher));

  rt.global().setProperty(rt, "__OPSQLiteProxy", std::move(module));
}

} // namespace opsqlite
