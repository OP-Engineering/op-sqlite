#include "OPDB.hpp"
#if OP_SQLITE_USE_LIBSQL
#include "libsql/bridge.hpp"
#else
#include "OPBridge.hpp"
#endif
#include "utils.hpp"

namespace opsqlite {

namespace jsi = facebook::jsi;

#ifdef OP_SQLITE_USE_TURSO
namespace {

std::string turso_remote_db_name(const std::string &url) {
  return "turso_remote_" + std::to_string(std::hash<std::string>{}(url)) +
         ".sqlite";
}

} // namespace
#endif

#if !defined(OP_SQLITE_USE_LIBSQL) && !defined(OP_SQLITE_USE_TURSO)
namespace {

std::string opdb_operation_to_string(int operation_type) {
  switch (operation_type) {
  case SQLITE_INSERT:
    return "INSERT";
  case SQLITE_DELETE:
    return "DELETE";
  case SQLITE_UPDATE:
    return "UPDATE";
  default:
    throw std::runtime_error("Unknown SQLite operation on hook");
  }
}

void opdb_update_callback(void *db_ptr, int operation_type,
                          [[maybe_unused]] char const *database,
                          char const *table, sqlite3_int64 row_id) {
  auto opdb = reinterpret_cast<OPDB *>(db_ptr);
  opdb->on_update(std::string(table),
                  opdb_operation_to_string(operation_type), row_id);
}

int opdb_commit_callback(void *db_ptr) {
  auto opdb = reinterpret_cast<OPDB *>(db_ptr);
  opdb->on_commit();
  return 0;
}

void opdb_rollback_callback(void *db_ptr) {
  auto opdb = reinterpret_cast<OPDB *>(db_ptr);
  opdb->on_rollback();
}

} // namespace
#endif

OPDB::OPDB(const std::string &name, const std::string &base_path,
           const std::string &sqlite_vec_path,
           const std::string &encryption_key)
    : base_path(base_path), db_name(name), delete_db_name(name) {
  thread_pool = std::make_shared<ThreadPool>();

#ifdef OP_SQLITE_USE_SQLCIPHER
  db = opsqlite_open(db_name, base_path, sqlite_vec_path, encryption_key);
#elif OP_SQLITE_USE_LIBSQL
  db = opsqlite_libsql_open(db_name, base_path);
#else
  db = opsqlite_open(db_name, base_path, sqlite_vec_path);
#endif
}

#ifdef OP_SQLITE_USE_LIBSQL
OPDB::OPDB(const std::string &url, const std::string &auth_token)
    : db_name(url) {
  thread_pool = std::make_shared<ThreadPool>();
  db = opsqlite_libsql_open_remote(url, auth_token);
}

OPDB::OPDB(const std::string &name, const std::string &path,
           const std::string &url, const std::string &auth_token,
           int sync_interval, bool offline,
           const std::string &encryption_key,
           const std::string &remote_encryption_key)
    : base_path(path), db_name(name), delete_db_name(name) {
  thread_pool = std::make_shared<ThreadPool>();
  db = opsqlite_libsql_open_sync(name, path, url, auth_token, sync_interval,
                                 offline, encryption_key,
                                 remote_encryption_key);
}
#elif defined(OP_SQLITE_USE_TURSO)
OPDB::OPDB(const std::string &url, const std::string &auth_token,
           const std::string &base_path)
    : base_path(base_path), db_name(url),
      delete_db_name(turso_remote_db_name(url)) {
  thread_pool = std::make_shared<ThreadPool>();
  db = opsqlite_open_remote(url, auth_token, base_path);
}

OPDB::OPDB(const std::string &name, const std::string &path,
           const std::string &url, const std::string &auth_token,
           const std::string &remote_encryption_key)
    : base_path(path), db_name(name), delete_db_name(name) {
  thread_pool = std::make_shared<ThreadPool>();
  db = opsqlite_open_sync(name, path, url, auth_token,
                          remote_encryption_key);
}
#endif

#ifdef OP_SQLITE_USE_LIBSQL
void OPDB::flush_pending_reactive_queries(
    const std::shared_ptr<jsi::Value> &resolve) {
  invoker->invokeAsync([resolve](jsi::Runtime &rt) {
    resolve->asObject(rt).asFunction(rt).call(rt, {});
  });
}
#elif defined(OP_SQLITE_USE_TURSO)
void OPDB::flush_pending_reactive_queries(
    const std::shared_ptr<jsi::Value> &resolve) {
  invoker->invokeAsync([resolve](jsi::Runtime &rt) {
    resolve->asObject(rt).asFunction(rt).call(rt, {});
  });
}
#else
void OPDB::flush_pending_reactive_queries(
    const std::shared_ptr<jsi::Value> &resolve) {
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

void OPDB::on_commit() {
  auto callback = commit_hook_callback;
  if (callback == nullptr) {
    return;
  }

  invoker->invokeAsync([callback](jsi::Runtime &rt) {
    callback->asObject(rt).asFunction(rt).call(rt);
  });
}

void OPDB::on_rollback() {
  auto callback = rollback_hook_callback;
  if (callback == nullptr) {
    return;
  }

  invoker->invokeAsync([callback](jsi::Runtime &rt) {
    callback->asObject(rt).asFunction(rt).call(rt);
  });
}

void OPDB::on_update(const std::string &table, const std::string &operation,
                     long long row_id) {
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

    if (query == nullptr) {
      continue;
    }

    if (query->discriminators.empty()) {
      continue;
    }

    bool shouldFire = false;

    for (const auto &discriminator : query->discriminators) {
      if (discriminator.table != table) {
        continue;
      }

      if (discriminator.ids.empty()) {
        shouldFire = true;
        break;
      }

      for (const auto &discriminator_id : discriminator.ids) {
        if (row_id == discriminator_id) {
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

void OPDB::auto_register_update_hook() {
  if (update_hook_callback == nullptr && reactive_queries.empty() &&
      is_update_hook_registered) {
    sqlite3_update_hook(db, nullptr, nullptr);
    is_update_hook_registered = false;
    return;
  }

  if (is_update_hook_registered) {
    return;
  }

  sqlite3_update_hook(db, &opdb_update_callback, this);
  is_update_hook_registered = true;
}

void OPDB::register_commit_hook() {
  sqlite3_commit_hook(db, &opdb_commit_callback, this);
}

void OPDB::deregister_commit_hook() { sqlite3_commit_hook(db, nullptr, nullptr); }

void OPDB::register_rollback_hook() {
  sqlite3_rollback_hook(db, &opdb_rollback_callback, this);
}

void OPDB::deregister_rollback_hook() {
  sqlite3_rollback_hook(db, nullptr, nullptr);
}
#endif

void OPDB::invalidate() {
  if (invalidated) {
    return;
  }

  invalidated = true;
  thread_pool->waitFinished();

#ifdef OP_SQLITE_USE_LIBSQL
  opsqlite_libsql_close(db);
#else
  if (db != nullptr) {
    opsqlite_close(db);
    db = nullptr;
  }
#endif
}

OPDB::~OPDB() { invalidate(); }

} // namespace opsqlite
