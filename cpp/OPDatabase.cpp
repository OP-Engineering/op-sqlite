#include "OPDatabase.hpp"
#include "PreparedStatementHostObject.hpp"
#if OP_SQLITE_USE_LIBSQL
#include "libsql/bridge.hpp"
#else
#include "bridge.hpp"
#endif
#include "logs.h"
#include "macros.hpp"
#include "utils.hpp"
#include <functional>
#include <iostream>
#include <utility>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

#ifdef OP_SQLITE_USE_LIBSQL
void OPDatabase::flush_pending_reactive_queries(
    const std::shared_ptr<jsi::Value> &resolve) {
  if (alive != nullptr && !alive->load()) {
    return;
  }
  invoker->invokeAsync([resolve](jsi::Runtime &rt) {
    resolve->asObject(rt).asFunction(rt).call(rt, {});
  });
}
#elif defined(OP_SQLITE_USE_TURSO)

std::string turso_remote_db_name(const std::string &url) {
  return "turso_remote_" + std::to_string(std::hash<std::string>{}(url)) +
         ".sqlite";
}

void OPDatabase::flush_pending_reactive_queries(
    const std::shared_ptr<jsi::Value> &resolve) {
  if (alive != nullptr && !alive->load()) {
    return;
  }
  invoker->invokeAsync([resolve](jsi::Runtime &rt) {
    resolve->asObject(rt).asFunction(rt).call(rt, {});
  });
}
#else
void OPDatabase::flush_pending_reactive_queries(
    const std::shared_ptr<jsi::Value> &resolve) {
  if (alive != nullptr && !alive->load()) {
    return;
  }
  for (const auto &query_ptr : pending_reactive_queries) {
    auto query = query_ptr.get();

    std::vector<DumbHostObject> results;
    std::shared_ptr<std::vector<SmartHostObject>> metadata =
        std::make_shared<std::vector<SmartHostObject>>();

    auto status = opsqlite_execute_prepared_statement(db, query->stmt, &results,
                                                      metadata);

    invoker->invokeAsync(
        [results = std::make_shared<std::vector<DumbHostObject>>(results),
         callback = query->callback, metadata,
         status = std::move(status)](jsi::Runtime &rt) {
          auto jsiResult = create_result(rt, status, results.get(), metadata);
          callback->asObject(rt).asFunction(rt).call(rt, jsiResult);
        });
  }

  pending_reactive_queries.clear();

  invoker->invokeAsync([resolve](jsi::Runtime &rt) {
    resolve->asObject(rt).asFunction(rt).call(rt, {});
  });
}

void OPDatabase::on_commit() {
  if (alive != nullptr && !alive->load()) {
    return;
  }
  invoker->invokeAsync([this](jsi::Runtime &rt) {
    commit_hook_callback->asObject(rt).asFunction(rt).call(rt);
  });
}

void OPDatabase::on_rollback() {
  if (alive != nullptr && !alive->load()) {
    return;
  }
  invoker->invokeAsync([this](jsi::Runtime &rt) {
    rollback_hook_callback->asObject(rt).asFunction(rt).call(rt);
  });
}

void OPDatabase::on_update(const std::string &table,
                             const std::string &operation, long long row_id) {
  if (alive != nullptr && !alive->load()) {
    return;
  }

  if (update_hook_callback != nullptr) {
    invoker->invokeAsync([callback = update_hook_callback, table, operation,
                          row_id](jsi::Runtime &rt) {
      auto res = jsi::Object(rt);
      res.setProperty(rt, "table", jsi::String::createFromUtf8(rt, table));
      res.setProperty(rt, "operation",
                      jsi::String::createFromUtf8(rt, operation));
      res.setProperty(rt, "rowId", jsi::Value(static_cast<double>(row_id)));

      callback->asObject(rt).asFunction(rt).call(rt, res);
    });
  }

  for (const auto &query_ptr : reactive_queries) {
    auto query = query_ptr.get();

    // The JS environment might have cleared the query while the update was
    // queued For now this seems to prevent a EXC_BAD_ACCESS
    if (query == nullptr) {
      continue;
    }

    if (query->discriminators.empty()) {
      continue;
    }

    bool shouldFire = false;

    for (const auto &discriminator : query->discriminators) {
      // Tables don't match then skip
      if (discriminator.table != table) {
        continue;
      }

      // If no ids are specified, then we should fire
      if (discriminator.ids.empty()) {
        shouldFire = true;
        break;
      }

      // If ids are specified, then we should check if the rowId matches
      for (const auto &discrimator_id : discriminator.ids) {
        if (row_id == discrimator_id) {
          shouldFire = true;
          break;
        }
      }
    }

    if (shouldFire) {
      pending_reactive_queries.insert(query_ptr);
    }
  }
}

void OPDatabase::sync_update_hook_registration() {
  if (invalidated || db == nullptr) {
    return;
  }

  if (update_hook_callback == nullptr && reactive_queries.empty() &&
      is_update_hook_registered) {
    opsqlite_deregister_update_hook(db);
    is_update_hook_registered = false;
    return;
  }

  if (is_update_hook_registered) {
    return;
  }

  opsqlite_register_update_hook(db, this);
  is_update_hook_registered = true;
}
#endif

void OPDatabase::throw_if_closed(const char *function_name) const {
  if (invalidated) {
    throw std::runtime_error(std::string("[op-sqlite][") + function_name +
                             "] database is closed");
  }
}

void OPDatabase::release_hooks() {
  reactive_queries.clear();
  pending_reactive_queries.clear();
  update_hook_callback = nullptr;
  commit_hook_callback = nullptr;
  rollback_hook_callback = nullptr;
  is_update_hook_registered = false;
}

//    _____                _                   _
//   / ____|              | |                 | |
//  | |     ___  _ __  ___| |_ _ __ _   _  ___| |_ ___  _ __
//  | |    / _ \| '_ \/ __| __| '__| | | |/ __| __/ _ \| '__|
//  | |___| (_) | | | \__ \ |_| |  | |_| | (__| || (_) | |
//   \_____\___/|_| |_|___/\__|_|   \__,_|\___|\__\___/|_|
#ifdef OP_SQLITE_USE_LIBSQL
// Remote connection constructor
OPDatabase::OPDatabase(jsi::Runtime &rt, jsi::Object &js_object,
                           std::string &url, std::string &auth_token)
    : db_name(url) {
  thread_pool = std::make_shared<ThreadPool>();
  db = opsqlite_libsql_open_remote(url, auth_token);

  create_jsi_functions(rt, js_object);
}

// Sync connection constructor
OPDatabase::OPDatabase(jsi::Runtime &rt, jsi::Object &js_object,
                           std::string &db_name, std::string &path,
                           std::string &url, std::string &auth_token,
                           int sync_interval, bool offline,
                           std::string &encryption_key,
                           std::string &remote_encryption_key)
    : base_path(path), db_name(db_name), delete_db_name(db_name) {

  thread_pool = std::make_shared<ThreadPool>();

  db =
      opsqlite_libsql_open_sync(db_name, path, url, auth_token, sync_interval,
                                offline, encryption_key, remote_encryption_key);

  create_jsi_functions(rt, js_object);
}

#elif defined(OP_SQLITE_USE_TURSO)
// Remote connection constructor
OPDatabase::OPDatabase(jsi::Runtime &rt, jsi::Object &js_object,
                           std::string &url, std::string &auth_token,
                           std::string &base_path)
    : base_path(base_path), db_name(url),
      delete_db_name(turso_remote_db_name(url)) {
  thread_pool = std::make_shared<ThreadPool>();
  db = opsqlite_open_remote(url, auth_token, base_path);

  create_jsi_functions(rt, js_object);
}

// Sync connection constructor
OPDatabase::OPDatabase(jsi::Runtime &rt, jsi::Object &js_object,
                           std::string &db_name, std::string &path,
                           std::string &url, std::string &auth_token,
                           std::string &remote_encryption_key)
    : base_path(path), db_name(db_name), delete_db_name(db_name) {

  thread_pool = std::make_shared<ThreadPool>();

  db =
      opsqlite_open_sync(db_name, path, url, auth_token, remote_encryption_key);

  create_jsi_functions(rt, js_object);
}

#endif

OPDatabase::OPDatabase(jsi::Runtime &rt, jsi::Object &js_object,
                           std::string &base_path, std::string &db_name,
                           std::string &path, bool readOnly,
                           bool failOnCreate, std::string &encryption_key)
    : base_path(base_path), db_name(db_name), delete_db_name(db_name) {
  thread_pool = std::make_shared<ThreadPool>();

#ifdef OP_SQLITE_USE_SQLCIPHER
  db = opsqlite_open(db_name, path, readOnly, failOnCreate, encryption_key);
#elif OP_SQLITE_USE_LIBSQL
  if (readOnly) {
    throw std::runtime_error("libsql does not support read-only databases.");
  }
  db = opsqlite_libsql_open(db_name, path, failOnCreate);
#else
  db = opsqlite_open(db_name, path, readOnly, failOnCreate);
#endif
  create_jsi_functions(rt, js_object);
};

void OPDatabase::create_jsi_functions(jsi::Runtime &rt,
                                        jsi::Object &js_object) {
  js_object.setProperty(rt, "attach", HFN(this) {
    throw_if_closed("attach");

    std::string secondary_db_path = std::string(base_path);

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

    // Reject zero bytes uniformly: libsql's libsql_bind_string takes a
    // C-string and would silently truncate at the first zero byte;
    // SQLite and Turso bind with explicit lengths. Failing loudly across
    // all backends keeps behaviour consistent.
    if (secondary_db_name.find('\0') != std::string::npos) {
      throw std::runtime_error("[op-sqlite] attach secondaryDbFileName must "
                               "not contain a zero byte");
    }
    if (alias.find('\0') != std::string::npos) {
      throw std::runtime_error(
          "[op-sqlite] attach alias must not contain a zero byte");
    }

#ifdef OP_SQLITE_USE_LIBSQL
    opsqlite_libsql_attach(db, secondary_db_path, secondary_db_name, alias);
#else
    opsqlite_attach(db, secondary_db_path, secondary_db_name, alias);
#endif

    return {};
  }));

  js_object.setProperty(rt, "detach", HFN(this) {
    throw_if_closed("detach");

    if (!args[0].isString()) {
      throw std::runtime_error("[op-sqlite] alias must be a strings");
    }

    std::string alias = args[0].asString(rt).utf8(rt);
    if (alias.find('\0') != std::string::npos) {
      throw std::runtime_error(
          "[op-sqlite] detach alias must not contain a zero byte");
    }
#ifdef OP_SQLITE_USE_LIBSQL
    opsqlite_libsql_detach(db, alias);
#else
    opsqlite_detach(db, alias);
#endif

    return {};
  }));

  js_object.setProperty(rt, "close", HFN(this) {
    invalidated = true;
    // Abort pending native SQLite work before waiting on the thread pool.
#if !defined(OP_SQLITE_USE_LIBSQL) && !defined(OP_SQLITE_USE_TURSO)
    if (db != nullptr) {
      sqlite3_interrupt(db);
    }
#endif
    // Drain any in-flight async queries before closing the db handle.
    // Without this, a queued/running execute() on the thread pool may
    // dereference the freed sqlite3* pointer → heap corruption / SIGABRT.
    thread_pool->wait_finished();
    release_hooks();
#ifdef OP_SQLITE_USE_LIBSQL
    opsqlite_libsql_close(db);
    db = {};
#else
    opsqlite_close(db);
    db = nullptr;
#endif

    return {};
  }));

  js_object.setProperty(rt, "interrupt", HFN(this) {
    if (invalidated) {
      throw std::runtime_error("[op-sqlite][interrupt] database is closed");
    }

#ifdef OP_SQLITE_USE_LIBSQL
    throw std::runtime_error("[op-sqlite][interrupt] sqlite3_interrupt is not "
                             "supported with libsql");
#elif defined(OP_SQLITE_USE_TURSO)
    throw std::runtime_error("[op-sqlite][interrupt] sqlite3_interrupt is not "
                             "supported with Turso");
#else
    if (db == nullptr) {
      throw std::runtime_error("[op-sqlite][interrupt] database is null");
    }

    sqlite3_interrupt(db);
    return {};
#endif
  }));

  js_object.setProperty(rt, "delete", HFN(this) {
    if (count != 0) {
      throw std::runtime_error("[op-sqlite] Delete no longer takes arguments");
    }

    invalidated = true;
    // Abort pending native SQLite work before waiting on the thread pool.
#if !defined(OP_SQLITE_USE_LIBSQL) && !defined(OP_SQLITE_USE_TURSO)
    if (db != nullptr) {
      sqlite3_interrupt(db);
    }
#endif
    // Drain any in-flight async queries before closing/removing the db handle.
    // Without this, queued/running work may dereference a freed sqlite handle.
    thread_pool->wait_finished();

    if (delete_db_name.empty()) {
      throw std::runtime_error("[op-sqlite][delete] delete() is not supported "
                               "for remote-only databases");
    }

    release_hooks();
#ifdef OP_SQLITE_USE_LIBSQL
    opsqlite_libsql_remove(db, delete_db_name, base_path);
#else
    auto *closing_db = db;
    db = nullptr;
    opsqlite_remove(closing_db, delete_db_name, base_path);
#endif

    return {};
  }));

  js_object.setProperty(rt, "executeRaw", HFN(this) {
    throw_if_closed("executeRaw");

    const std::string query = args[0].asString(rt).utf8(rt);
    const std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                              ? to_variant_vec(rt, args[1])
                                              : std::vector<JSVariant>();

    return promisify(
        rt, thread_pool,
        [this, query, params]() {
          std::vector<std::vector<JSVariant>> results;
#ifdef OP_SQLITE_USE_LIBSQL
          auto status =
              opsqlite_libsql_execute_raw(db, query, &params, &results);
#else
          auto status = opsqlite_execute_raw(db, query, &params, &results);
#endif
          return std::make_tuple(status, results);
        },
        [](jsi::Runtime &rt, std::any prev) {
          auto tuple = std::any_cast<
              std::tuple<BridgeResult, std::vector<std::vector<JSVariant>>>>(
              std::move(prev));

          return create_raw_result(rt, std::get<0>(tuple), &std::get<1>(tuple));
        });
  }));

  js_object.setProperty(rt, "executeSync", HFN(this) {
    throw_if_closed("executeSync");

    std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params;

    if (count == 2 && !args[1].isNull() && !args[1].isUndefined()) {
      params = to_variant_vec(rt, args[1]);
    }
#ifdef OP_SQLITE_USE_LIBSQL
    auto status = opsqlite_libsql_execute(db, query, &params);
#else
    auto status = opsqlite_execute(db, query, &params);
#endif

    return create_js_rows(rt, status);
  }));

  js_object.setProperty(rt, "executeRawSync", HFN(this) {
    throw_if_closed("executeRawSync");

    const std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                        ? to_variant_vec(rt, args[1])
                                        : std::vector<JSVariant>();

    std::vector<std::vector<JSVariant>> results;

#ifdef OP_SQLITE_USE_LIBSQL
    auto status = opsqlite_libsql_execute_raw(db, query, &params, &results);
#else
    auto status = opsqlite_execute_raw(db, query, &params, &results);
#endif

    return create_raw_result(rt, status, &results);
  }));

  js_object.setProperty(rt, "execute", HFN(this) {
    throw_if_closed("execute");

    const std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                        ? to_variant_vec(rt, args[1])
                                        : std::vector<JSVariant>();

    return promisify(
        rt, thread_pool,
        [this, query, params]() {
#ifdef OP_SQLITE_USE_LIBSQL
          auto status = opsqlite_libsql_execute(db, query, &params);
#else
          auto status = opsqlite_execute(db, query, &params);
#endif
          return status;
        },
        [](jsi::Runtime &rt, std::any prev) {
          auto status = std::any_cast<BridgeResult>(std::move(prev));
          return create_js_rows(rt, status);
        });
  }));

  js_object.setProperty(rt, "executeWithHostObjects", HFN(this) {
    throw_if_closed("executeWithHostObjects");

    const std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                        ? to_variant_vec(rt, args[1])
                                        : std::vector<JSVariant>();

    return promisify(
        rt, thread_pool,
        [this, query, params]() {
          std::vector<DumbHostObject> results;
          std::shared_ptr<std::vector<SmartHostObject>> metadata =
              std::make_shared<std::vector<SmartHostObject>>();
#ifdef OP_SQLITE_USE_LIBSQL
          auto status = opsqlite_libsql_execute_with_host_objects(
              db, query, &params, &results, metadata);
#else
          auto status = opsqlite_execute_host_objects(db, query, &params,
                                                      &results, metadata);
#endif
          return std::make_tuple(status, results, metadata);
        },
        [](jsi::Runtime &rt, std::any prev) {
          auto tuple = std::any_cast<
              std::tuple<BridgeResult, std::vector<DumbHostObject>,
                         std::shared_ptr<std::vector<SmartHostObject>>>>(
              std::move(prev));
          auto results = std::make_shared<std::vector<DumbHostObject>>(
              std::move(std::get<1>(tuple)));
          return create_result(rt, std::get<0>(tuple), results.get(),
                               std::get<2>(tuple));
        });
  }));

  js_object.setProperty(rt, "executeBatch", HFN(this) {
    throw_if_closed("executeBatch");

    if (count < 1) {
      throw std::runtime_error(
          "[op-sqlite][executeAsyncBatch] Incorrect parameter count");
    }

    const jsi::Value &params = args[0];

    if (params.isNull() || params.isUndefined()) {
      throw std::runtime_error(
          "[op-sqlite][executeAsyncBatch] - An array of SQL "
          "commands or parameters is needed");
    }

    const jsi::Array &batchParams = params.asObject(rt).asArray(rt);

    std::vector<BatchArguments> commands;
    to_batch_arguments(rt, batchParams, &commands);

    return promisify(
        rt, thread_pool,
        [this, commands]() {
#ifdef OP_SQLITE_USE_LIBSQL
          auto batchResult = opsqlite_libsql_execute_batch(db, &commands);
#else
          auto batchResult = opsqlite_execute_batch(db, &commands);
#endif
          return batchResult;
        },
        [](jsi::Runtime &rt, std::any prev) {
          auto batchResult = std::any_cast<BatchResult>(std::move(prev));
          auto res = jsi::Object(rt);
          res.setProperty(rt, "rowsAffected",
                          jsi::Value(batchResult.affectedRows));
          return res;
        });
  }));

#if defined(OP_SQLITE_USE_LIBSQL) || defined(OP_SQLITE_USE_TURSO)
  js_object.setProperty(rt, "sync", HFN(this) {
    throw_if_closed("sync");

#ifdef OP_SQLITE_USE_LIBSQL
    opsqlite_libsql_sync(db);
#else
    opsqlite_sync(db);
#endif
    return {};
  }));

#ifdef OP_SQLITE_USE_LIBSQL

  js_object.setProperty(rt, "setReservedBytes", HFN(this) {
    throw_if_closed("setReservedBytes");

    auto reserved_bytes = static_cast<int32_t>(args[0].asNumber());
    opsqlite_libsql_set_reserved_bytes(db, reserved_bytes);
    return {};
  }));

  js_object.setProperty(rt, "getReservedBytes", HFN(this) {
    throw_if_closed("getReservedBytes");

    return {opsqlite_libsql_get_reserved_bytes(db)};
  }));
#endif

#endif

#if !defined(OP_SQLITE_USE_LIBSQL) && !defined(OP_SQLITE_USE_TURSO)
  js_object.setProperty(rt, "loadFile", HFN(this) {
    throw_if_closed("loadFile");

    if (count < 1) {
      throw std::runtime_error(
          "[op-sqlite][loadFile] Incorrect parameter count");
    }

    const std::string sqlFileName = args[0].asString(rt).utf8(rt);

    return promisify(
        rt, thread_pool,
        [this, sqlFileName]() { return import_sql_file(db, sqlFileName); },
        [](jsi::Runtime &rt, std::any prev) {
          auto result = std::any_cast<BatchResult>(std::move(prev));
          auto res = jsi::Object(rt);
          res.setProperty(rt, "rowsAffected", jsi::Value(result.affectedRows));
          res.setProperty(rt, "commands", jsi::Value(result.commands));
          return res;
        });
  }));

  js_object.setProperty(rt, "updateHook", HFN(this) {
    throw_if_closed("updateHook");

    auto callback = std::make_shared<jsi::Value>(rt, args[0]);

    if (callback->isUndefined() || callback->isNull()) {
      update_hook_callback = nullptr;
    } else {
      update_hook_callback = callback;
    }

    sync_update_hook_registration();
    return {};
  }));

  js_object.setProperty(rt, "commitHook", HFN(this) {
    throw_if_closed("commitHook");

    if (count < 1) {
      throw std::runtime_error("[op-sqlite][commitHook] callback needed");
    }

    auto callback = std::make_shared<jsi::Value>(rt, args[0]);
    if (callback->isUndefined() || callback->isNull()) {
      opsqlite_deregister_commit_hook(db);
      return {};
    }
    commit_hook_callback = callback;
    opsqlite_register_commit_hook(db, this);

    return {};
  }));

  js_object.setProperty(rt, "rollbackHook", HFN(this) {
    throw_if_closed("rollbackHook");

    if (count < 1) {
      throw std::runtime_error("[op-sqlite][rollbackHook] callback needed");
    }

    auto callback = std::make_shared<jsi::Value>(rt, args[0]);

    if (callback->isUndefined() || callback->isNull()) {
      opsqlite_deregister_rollback_hook(db);
      return {};
    }
    rollback_hook_callback = callback;

    opsqlite_register_rollback_hook(db, this);
    return {};
  }));

  js_object.setProperty(rt, "loadExtension", HFN(this) {
    throw_if_closed("loadExtension");

    auto path = args[0].asString(rt).utf8(rt);
    std::string entry_point;
    if (count > 1 && args[1].isString()) {
      entry_point = args[1].asString(rt).utf8(rt);
    }

    opsqlite_load_extension(db, path, entry_point);
    return {};
  }));

  js_object.setProperty(rt, "reactiveExecute", HFN(this) {
    throw_if_closed("reactiveExecute");

    auto query = args[0].asObject(rt);

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

    sync_update_hook_registration();

    auto weak_self = weak_from_this();

    auto unsubscribe = HFN2(weak_self, reactiveQuery) {
      auto self = weak_self.lock();
      if (self == nullptr) {
        return {};
      }
      auto it = std::find(self->reactive_queries.begin(),
                          self->reactive_queries.end(), reactiveQuery);
      if (it != self->reactive_queries.end()) {
        self->reactive_queries.erase(it);
      }
      self->sync_update_hook_registration();
      return {};
    });

    return unsubscribe;
  }));
#endif

  js_object.setProperty(rt, "prepareStatement", HFN(this) {
    throw_if_closed("prepareStatement");

    auto query = args[0].asString(rt).utf8(rt);
#ifdef OP_SQLITE_USE_LIBSQL
    libsql_stmt_t statement = opsqlite_libsql_prepare_statement(db, query);
#else
    sqlite3_stmt *statement = opsqlite_prepare_statement(db, query);
#endif
    auto preparedStatementHostObject =
        std::make_shared<PreparedStatementHostObject>(db, statement,
                                                      thread_pool);

    return jsi::Object::createFromHostObject(rt, preparedStatementHostObject);
  }));

  js_object.setProperty(rt, "getDbPath", HFN(this) {
    std::string path = std::string(base_path);

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

    auto result = opsqlite_get_db_path(db_name, path);
    return jsi::String::createFromUtf8(rt, result);
  }));

  js_object.setProperty(rt, "flushPendingReactiveQueries", HFN(this) {
    throw_if_closed("flushPendingReactiveQueries");

    auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
    auto promise = promiseCtr.callAsConstructor(rt, HFN(this) {
      auto resolve = std::make_shared<jsi::Value>(rt, args[0]);

      auto task = [this, resolve]() {
        flush_pending_reactive_queries(resolve);
      };

      thread_pool->queue_work(task);

      return {};
    }));

    return promise;
  }));
}

void OPDatabase::invalidate() {
  if (invalidated) {
    return;
  }

  invalidated = true;

  // Abort whatever is currently inside sqlite3_step so the drain below can
  // actually finish. Parity with the close and delete host functions, which
  // already do this. Without it a long running query holds the pool past React
  // Native's module invalidation budget, after which the runtime is destroyed
  // anyway and the drain has bought nothing.
#if !defined(OP_SQLITE_USE_LIBSQL) && !defined(OP_SQLITE_USE_TURSO)
  if (db != nullptr) {
    sqlite3_interrupt(db);
  }
#endif

  // Drain in-flight thread pool work before closing the db handle.
  thread_pool->wait_finished();
  release_hooks();

#ifdef OP_SQLITE_USE_LIBSQL
  opsqlite_libsql_close(db);
#else
  if (db != nullptr) {
    opsqlite_close(db);
    db = nullptr;
  }
#endif
}

OPDatabase::~OPDatabase() { invalidate(); }

} // namespace opsqlite
