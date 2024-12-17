#pragma once

#include "ThreadPool.h"
#include "sqlite3.h"
#include "types.h"
#include <ReactCommon/CallInvoker.h>
#include <jsi/jsi.h>
#include <set>
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
  sqlite3_stmt *stmt;
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
               std::shared_ptr<react::CallInvoker> invoker,
               std::shared_ptr<ThreadPool> thread_pool);

  // Constructor for a local database with remote sync
  DBHostObject(jsi::Runtime &rt, std::shared_ptr<react::CallInvoker> invoker,
               std::string &db_name, std::string &path, std::string &url,
               std::string &auth_token, int sync_interval);
#endif

  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt);
  jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propNameID);
  void set(jsi::Runtime &rt, const jsi::PropNameID &name,
           const jsi::Value &value);
  void on_update(std::string table, std::string operation, int rowid);
  void on_commit();
  void on_rollback();
  void invalidate();
  ~DBHostObject();

private:
  std::set<std::shared_ptr<ReactiveQuery>> pending_reactive_queries;
  void auto_register_update_hook();
  void create_jsi_functions();
  void flush_pending_reactive_queries(std::shared_ptr<jsi::Value> resolve);

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
  libsql_database_t db;
  libsql_connection_t c;
#else
  sqlite3 *db;
#endif
};

} // namespace opsqlite
