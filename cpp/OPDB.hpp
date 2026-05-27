#pragma once

#include "OPThreadPool.h"
#include "types.hpp"
#include <jsi/jsi.h>
#include <memory>
#include <set>
#ifdef OP_SQLITE_USE_LIBSQL
#include "libsql/OPBridge.hpp"
#else
#ifdef __ANDROID__
#include "sqlite3.h"
#else
#include <sqlite3.h>
#endif
#endif
#include <string>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;

struct OPPendingReactiveInvocation {
  std::string db_name;
  std::string table;
  std::string rowid;
};

struct OPTableRowDiscriminator {
  std::string table;
  std::vector<int> ids;
};

struct OPReactiveQuery {
#if !defined(OP_SQLITE_USE_LIBSQL) && !defined(OP_SQLITE_USE_TURSO)
  sqlite3_stmt *stmt;
#endif
  std::vector<OPTableRowDiscriminator> discriminators;
  std::shared_ptr<jsi::Value> callback;
};

class OPDB : public jsi::NativeState {
public:
  OPDB(const std::string &name, const std::string &base_path,
       const std::string &sqlite_vec_path,
       const std::string &encryption_key);
#ifdef OP_SQLITE_USE_LIBSQL
  OPDB(const std::string &url, const std::string &auth_token);
  OPDB(const std::string &name, const std::string &path,
    const std::string &url, const std::string &auth_token,
    int sync_interval, bool offline,
    const std::string &encryption_key,
    const std::string &remote_encryption_key);
#elif defined(OP_SQLITE_USE_TURSO)
  OPDB(const std::string &url, const std::string &auth_token,
    const std::string &base_path);
  OPDB(const std::string &name, const std::string &path,
    const std::string &url, const std::string &auth_token,
    const std::string &remote_encryption_key);
#endif
  ~OPDB() override;

  void auto_register_update_hook();
  void register_commit_hook();
  void deregister_commit_hook();
  void register_rollback_hook();
  void deregister_rollback_hook();
  void flush_pending_reactive_queries(
      const std::shared_ptr<jsi::Value> &resolve);
  void on_update(const std::string &table, const std::string &operation,
                 long long row_id);
  void on_commit();
  void on_rollback();
  void invalidate();

  std::set<std::shared_ptr<OPReactiveQuery>> pending_reactive_queries;
  std::string base_path;
  std::shared_ptr<ThreadPool> thread_pool;
  std::string db_name;
  std::string delete_db_name;
  std::shared_ptr<jsi::Value> update_hook_callback;
  std::shared_ptr<jsi::Value> commit_hook_callback;
  std::shared_ptr<jsi::Value> rollback_hook_callback;
  std::vector<std::shared_ptr<OPReactiveQuery>> reactive_queries;
  std::vector<OPPendingReactiveInvocation> pending_reactive_invocations;
  bool is_update_hook_registered = false;
  bool invalidated = false;
#ifdef OP_SQLITE_USE_LIBSQL
  DB db;
#else
  sqlite3 *db;
#endif
};

} // namespace opsqlite


