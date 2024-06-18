#pragma once

#include "ThreadPool.h"
#include "sqlite3.h"
#include "types.h"
#include <ReactCommon/CallInvoker.h>
#include <any>
#include <jsi/jsi.h>
#include <unordered_map>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

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
  // Constructor for local databases
  DBHostObject(jsi::Runtime &rt, std::string &base_path,
               std::shared_ptr<react::CallInvoker> js_call_invoker,
               std::shared_ptr<ThreadPool> thread_pool, std::string &db_name,
               std::string &path, std::string &crsqlite_path,
               std::string &encryption_key);

#ifdef OP_SQLITE_USE_LIBSQL
  // Constructor for remoteOpen, purely for remote databases
  DBHostObject(jsi::Runtime &rt, std::string &url, std::string &auth_token,
               std::shared_ptr<react::CallInvoker> js_call_invoker,
               std::shared_ptr<ThreadPool> thread_pool);

  // Constructor for a local database with remote sync
  DBHostObject(jsi::Runtime &rt, std::shared_ptr<react::CallInvoker> invoker,
               std::shared_ptr<ThreadPool> thread_pool, std::string &db_name,
               std::string &path, std::string &url, std::string &auth_token);
#endif

  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt);
  jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propNameID);
  void set(jsi::Runtime &rt, const jsi::PropNameID &name,
           const jsi::Value &value);

private:
  void auto_register_update_hook();
  void create_jsi_functions();

  std::unordered_map<std::string, jsi::Value> function_map;
  std::string base_path;
  std::shared_ptr<jsi::Value> update_hook;
  std::shared_ptr<react::CallInvoker> jsCallInvoker;
  std::shared_ptr<ThreadPool> thread_pool;
  std::string db_name;
  std::shared_ptr<jsi::Value> update_hook_callback;
  std::shared_ptr<jsi::Value> commit_hook_callback;
  std::shared_ptr<jsi::Value> rollback_hook_callback;
  jsi::Runtime &rt;
  std::vector<std::shared_ptr<ReactiveQuery>> reactive_queries;
  bool is_update_hook_registered = false;
};

} // namespace opsqlite
