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

struct DB {
    libsql_database_t db;
    libsql_connection_t c;
};

std::string opsqlite_get_db_path(std::string const &name,
                                 std::string const &location);

DB opsqlite_libsql_open(std::string const &name, std::string const &path,
                        std::string const &crsqlitePath);

DB opsqlite_libsql_open_remote(std::string const &url,
                               std::string const &auth_token);

DB opsqlite_libsql_open_sync(std::string const &name, std::string const &path,
                             std::string const &url,
                             std::string const &auth_token, int sync_interval,
                             bool offline, std::string const &encryption_key,
                             std::string const &remote_encryption_key);

void opsqlite_libsql_close(DB &db);

void opsqlite_libsql_remove(DB &db, std::string const &name,
                            std::string const &path);

void opsqlite_libsql_attach(DB const &db, std::string const &docPath,
                            std::string const &databaseToAttach,
                            std::string const &alias);

void opsqlite_libsql_detach(DB const &db, std::string const &alias);

void opsqlite_libsql_sync(DB const &db);

BridgeResult opsqlite_libsql_execute(DB const &db, std::string const &query,
                                     const std::vector<JSVariant> *params);

BridgeResult opsqlite_libsql_execute_with_host_objects(
    DB const &db, std::string const &query,
    const std::vector<JSVariant> *params, std::vector<DumbHostObject> *results,
    const std::shared_ptr<std::vector<SmartHostObject>> &metadatas);

BridgeResult
opsqlite_libsql_execute_raw(DB const &db, std::string const &query,
                            const std::vector<JSVariant> *params,
                            std::vector<std::vector<JSVariant>> *results);

BatchResult
opsqlite_libsql_execute_batch(DB const &db,
                              const std::vector<BatchArguments> *commands);

libsql_stmt_t opsqlite_libsql_prepare_statement(DB const &db,
                                                std::string const &query);

void opsqlite_libsql_bind_statement(libsql_stmt_t stmt,
                                    const std::vector<JSVariant> *params);

BridgeResult opsqlite_libsql_execute_prepared_statement(
    DB const &db, libsql_stmt_t stmt, std::vector<DumbHostObject> *results,
    const std::shared_ptr<std::vector<SmartHostObject>> &metadatas);

} // namespace opsqlite
