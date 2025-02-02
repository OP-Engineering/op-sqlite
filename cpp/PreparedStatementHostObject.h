#pragma once

#include <ReactCommon/CallInvoker.h>
#include <jsi/jsi.h>
#include <memory>
#ifdef OP_SQLITE_USE_LIBSQL
#include "libsql.h"
#include "libsql/bridge.h"
#else
#include <sqlite3.h>
#endif
#include "OPThreadPool.h"
#include <string>
#include <utility>

namespace opsqlite {
namespace jsi = facebook::jsi;
namespace react = facebook::react;

class PreparedStatementHostObject : public jsi::HostObject {
  public:
#ifdef OP_SQLITE_USE_LIBSQL
    PreparedStatementHostObject(
        DB const &db, std::string name, libsql_stmt_t stmt,
        std::shared_ptr<react::CallInvoker> js_call_invoker,
        std::shared_ptr<ThreadPool> thread_pool)
        : _db(db), _name(std::move(name)), _stmt(stmt),
          _js_call_invoker(js_call_invoker), _thread_pool(thread_pool) {};
#else
    PreparedStatementHostObject(
        sqlite3 *db, std::string name, sqlite3_stmt *stmt,
        std::shared_ptr<react::CallInvoker> js_call_invoker,
        std::shared_ptr<ThreadPool> thread_pool)
        : _db(db), _name(std::move(name)), _stmt(stmt),
          _js_call_invoker(std::move(js_call_invoker)),
          _thread_pool(std::move(thread_pool)) {};
#endif
    ~PreparedStatementHostObject() override;

    std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt) override;

    jsi::Value get(jsi::Runtime &rt,
                   const jsi::PropNameID &propNameID) override;

  private:
    std::string _name;
#ifdef OP_SQLITE_USE_LIBSQL
    DB _db;
    libsql_stmt_t _stmt;
#else
    sqlite3 *_db;
    // This shouldn't be de-allocated until sqlite3_finalize is called on it
    sqlite3_stmt *_stmt;
#endif
    std::shared_ptr<react::CallInvoker> _js_call_invoker;
    std::shared_ptr<ThreadPool> _thread_pool;
};

} // namespace opsqlite
