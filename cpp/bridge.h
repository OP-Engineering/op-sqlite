#pragma once

#include "DumbHostObject.h"
#include "SmartHostObject.h"
#include "types.h"
#include "utils.h"
#ifdef __ANDROID__
#include "sqlite3.h"
#else
#include <sqlite3.h>
#endif
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;

/// Convenience types to avoid super long types
typedef std::function<void(std::string dbName, std::string tableName,
                           std::string operation, int rowId)>
    UpdateCallback;
typedef std::function<void(std::string dbName)> CommitCallback;
typedef std::function<void(std::string dbName)> RollbackCallback;

std::string opsqlite_get_db_path(std::string const &db_name,
                                 std::string const &location);

#ifdef OP_SQLITE_USE_SQLCIPHER
sqlite3 *opsqlite_open(std::string const &dbName, std::string const &path,
                       std::string const &crsqlite_path,
                       std::string const &sqlite_vec_path,
                       std::string const &encryption_key);
#else
sqlite3 *opsqlite_open(std::string const &name, std::string const &path,
                       [[maybe_unused]] std::string const &crsqlite_path,
                       std::string const &sqlite_vec_path);
#endif

void opsqlite_close(sqlite3 *db);

void opsqlite_remove(sqlite3 *db, std::string const &name,
                     std::string const &doc_path);

void opsqlite_attach(sqlite3 *db, std::string const &doc_path,
                     std::string const &secondary_db_name,
                     std::string const &alias);

void opsqlite_detach(sqlite3 *db, std::string const &alias);

BridgeResult opsqlite_execute(sqlite3 *db, std::string const &query,
                              const std::vector<JSVariant> *params);

BridgeResult opsqlite_execute_host_objects(
    sqlite3 *db, std::string const &query, const std::vector<JSVariant> *params,
    std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> &metadatas);

BatchResult opsqlite_execute_batch(sqlite3 *db,
                                   const std::vector<BatchArguments> *commands);

BridgeResult opsqlite_execute_raw(sqlite3 *db, std::string const &query,
                                  const std::vector<JSVariant> *params,
                                  std::vector<std::vector<JSVariant>> *results);

void opsqlite_register_update_hook(sqlite3 *db, void *db_host_object_ptr);
void opsqlite_deregister_update_hook(sqlite3 *db);
void opsqlite_register_commit_hook(sqlite3 *db, void *db_host_object_ptr);
void opsqlite_deregister_commit_hook(sqlite3 *db);
void opsqlite_register_rollback_hook(sqlite3 *db, void *db_host_object_ptr);
void opsqlite_deregister_rollback_hook(sqlite3 *db);

sqlite3_stmt *opsqlite_prepare_statement(sqlite3 *db, std::string const &query);

void opsqlite_bind_statement(sqlite3_stmt *statement,
                             const std::vector<JSVariant> *params);

BridgeResult opsqlite_execute_prepared_statement(
    sqlite3 *db, sqlite3_stmt *statement, std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> &metadatas);

void opsqlite_load_extension(sqlite3 *db, std::string &path,
                             std::string &entry_point);

} // namespace opsqlite
