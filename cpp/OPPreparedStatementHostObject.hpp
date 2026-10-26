#pragma once

#include <ReactCommon/CallInvoker.h>
#include <jsi/jsi.h>
#include <memory>
#ifdef OP_SQLITE_USE_LIBSQL
#include "libsql.h"
#include "libsql/OPLibsqlBridge.hpp"
#else
#ifdef __ANDROID__
#include "sqlite3.h"
#else
#include <sqlite3.h>
#endif
#endif
#include "OPWorkerThread.hpp"
#include <string>
#include <utility>

namespace opsqlite {
namespace jsi = facebook::jsi;
namespace react = facebook::react;

class OPDatabase;

class PreparedStatementHostObject : public jsi::HostObject {
public:
#ifdef OP_SQLITE_USE_LIBSQL
  PreparedStatementHostObject(std::weak_ptr<OPDatabase> database, DB const &db,
                              libsql_stmt_t stmt,
                              std::shared_ptr<WorkerThread> worker_thread)
      : _database(std::move(database)), _db(db), _stmt(stmt),
        _worker_thread(worker_thread) {};
#else
  PreparedStatementHostObject(std::weak_ptr<OPDatabase> database, sqlite3 *db,
                              sqlite3_stmt *stmt,
                              std::shared_ptr<WorkerThread> worker_thread)
      : _database(std::move(database)), _db(db), _stmt(stmt),
        _worker_thread(worker_thread) {};
#endif

  ~PreparedStatementHostObject() override;

  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt) override;

  jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propNameID) override;

private:
  // Throws the same error as OPDatabase::throw_if_closed once the database
  // this statement came from is closed (or gone): `_db` is no longer valid.
  void throw_if_closed(const char *function_name) const;

  std::weak_ptr<OPDatabase> _database;
#ifdef OP_SQLITE_USE_LIBSQL
  DB _db;
  libsql_stmt_t _stmt;
#else
  sqlite3 *_db;
  // This shouldn't be de-allocated until sqlite3_finalize is called on it
  sqlite3_stmt *_stmt;
#endif
  std::shared_ptr<WorkerThread> _worker_thread;
};

} // namespace opsqlite
