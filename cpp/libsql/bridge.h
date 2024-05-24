#pragma once

#include "DumbHostObject.h"
#include "SmartHostObject.h"
#include "libsql.h"
#include "types.h"
#include "utils.h"
#include <vector>

#define LIBSQL_INT 1
#define LIBSQL_FLOAT 2
#define LIBSQL_TEXT 3
#define LIBSQL_BLOB 4
#define LIBSQL_NULL 5

namespace opsqlite {

namespace jsi = facebook::jsi;

/// Convenience types to avoid super long types
typedef std::function<void(std::string dbName, std::string table_name,
                           std::string operation, int row_id)>
    UpdateCallback;
typedef std::function<void(std::string dbName)> CommitCallback;
typedef std::function<void(std::string dbName)> RollbackCallback;

std::string opsqlite_get_db_path(std::string const &name,
                                 std::string const &location);

BridgeResult opsqlite_libsql_open(std::string const &name,
                                  std::string const &path);

BridgeResult opsqlite_libsql_close(std::string const &name);

BridgeResult opsqlite_libsql_remove(std::string const &name,
                                    std::string const &path);

BridgeResult opsqlite_libsql_attach(std::string const &mainDBName,
                                    std::string const &docPath,
                                    std::string const &databaseToAttach,
                                    std::string const &alias);

BridgeResult opsqlite_libsql_detach(std::string const &mainDBName,
                                    std::string const &alias);

BridgeResult opsqlite_libsql_execute(
    std::string const &name, std::string const &query,
    const std::vector<JSVariant> *params, std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> metadatas);

BridgeResult
opsqlite_libsql_execute_raw(std::string const &dbName, std::string const &query,
                            const std::vector<JSVariant> *params,
                            std::vector<std::vector<JSVariant>> *results);

BatchResult
opsqlite_libsql_execute_batch(std::string const &name,
                              std::vector<BatchArguments> *commands);

// void opsqlite_close_all();

// BridgeResult opsqlite_register_update_hook(std::string const &dbName,
//                                            UpdateCallback const callback);
// BridgeResult opsqlite_deregister_update_hook(std::string const &dbName);
// BridgeResult opsqlite_register_commit_hook(std::string const &dbName,
//                                            CommitCallback const callback);
// BridgeResult opsqlite_deregister_commit_hook(std::string const &dbName);
// BridgeResult opsqlite_register_rollback_hook(std::string const &dbName,
//                                              RollbackCallback const
//                                              callback);
// BridgeResult opsqlite_deregister_rollback_hook(std::string const &dbName);

libsql_stmt_t opsqlite_libsql_prepare_statement(std::string const &name,
                                                std::string const &query);

void opsqlite_libsql_bind_statement(libsql_stmt_t stmt,
                                    const std::vector<JSVariant> *params);

BridgeResult opsqlite_libsql_execute_prepared_statement(
    std::string const &name, libsql_stmt_t stmt,
    std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> metadatas);

// BridgeResult opsqlite_load_extension(std::string const &db_name,
//                                      std::string &path,
//                                      std::string &entry_point);

} // namespace opsqlite
