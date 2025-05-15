#pragma once

#include "OPThreadPool.h"
#include "types.h"
#include <ReactCommon/CallInvoker.h>
#include <jsi/jsi.h>
#include <set>
#ifdef OP_SQLITE_USE_LIBSQL
#include "libsql/bridge.h"
#else
#include <sqlite3.h>
#endif
#include <unordered_map>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

struct PendingReactiveInvocation {
    std::string db_name;
    std::string table;
    std::string rowid;
};

struct TableRowDiscriminator {
    std::string table;
    std::vector<int> ids;
};

struct ReactiveQuery {
#ifndef OP_SQLITE_USE_LIBSQL
    sqlite3_stmt *stmt;
#endif
    std::vector<TableRowDiscriminator> discriminators;
    std::shared_ptr<jsi::Value> callback;
};

class JSI_EXPORT DBHostObject : public jsi::HostObject {
  public:
    // Normal constructor shared between all backends
    DBHostObject(jsi::Runtime &rt, std::string &base_path,
                 std::shared_ptr<react::CallInvoker> invoker,
                 std::string &db_name, std::string &path,
                 std::string &crsqlite_path, std::string &sqlite_vec_path,
                 std::string &encryption_key);

#ifdef OP_SQLITE_USE_LIBSQL
    // Constructor for remoteOpen, purely for remote databases
    DBHostObject(jsi::Runtime &rt, std::string &url, std::string &auth_token,
                 std::shared_ptr<react::CallInvoker> invoker);

    // Constructor for a local database with remote sync
    DBHostObject(jsi::Runtime &rt, std::shared_ptr<react::CallInvoker> invoker,
                 std::string &db_name, std::string &path, std::string &url,
                 std::string &auth_token, int sync_interval, bool offline);
#endif

    std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt) override;
    jsi::Value get(jsi::Runtime &rt,
                   const jsi::PropNameID &propNameID) override;
    void set(jsi::Runtime &rt, const jsi::PropNameID &name,
             const jsi::Value &value) override;
    void on_update(const std::string &table, const std::string &operation,
                   long long row_id);
    void on_commit();
    void on_rollback();
    void invalidate();
    ~DBHostObject() override;

  private:
    std::set<std::shared_ptr<ReactiveQuery>> pending_reactive_queries;
    void auto_register_update_hook();
    void create_jsi_functions();
    void
    flush_pending_reactive_queries(const std::shared_ptr<jsi::Value> &resolve);

    std::unordered_map<std::string, jsi::Value> function_map;
    std::string base_path;
    std::shared_ptr<react::CallInvoker> invoker;
    std::shared_ptr<ThreadPool> _thread_pool;
    std::string db_name;
    std::shared_ptr<jsi::Value> update_hook_callback;
    std::shared_ptr<jsi::Value> commit_hook_callback;
    std::shared_ptr<jsi::Value> rollback_hook_callback;
    jsi::Runtime &rt;
    std::vector<std::shared_ptr<ReactiveQuery>> reactive_queries;
    std::vector<PendingReactiveInvocation> pending_reactive_invocations;
    bool is_update_hook_registered = false;
    bool invalidated = false;
#ifdef OP_SQLITE_USE_LIBSQL
    DB db;
#else
    sqlite3 *db;
#endif
};

} // namespace opsqlite
