#ifndef bridge_h
#define bridge_h

#include "DumbHostObject.h"
#include "SmartHostObject.h"
#include "types.h"
#include "utils.h"
#include <sqlite3.h>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;

BridgeResult sqlite_open(std::string const &dbName, std::string const &dbPath);

BridgeResult sqlite_close(std::string const &dbName);

BridgeResult sqlite_remove(std::string const &dbName,
                           std::string const &docPath);

BridgeResult sqlite_attach(std::string const &mainDBName,
                           std::string const &docPath,
                           std::string const &databaseToAttach,
                           std::string const &alias);

BridgeResult sqlite_detach(std::string const &mainDBName,
                           std::string const &alias);

BridgeResult
sqlite_execute(std::string const &dbName, std::string const &query,
               const std::vector<JSVariant> *params,
               std::vector<DumbHostObject> *results,
               std::shared_ptr<std::vector<SmartHostObject>> metadatas);

BridgeResult sqlite_execute_literal(std::string const &dbName,
                                    std::string const &query);

void sqlite_close_all();

BridgeResult sqlite_register_update_hook(
    std::string const &dbName,
    std::function<void(std::string dbName, std::string tableName,
                       std::string operation, int rowId)> const callback);
BridgeResult sqlite_deregister_update_hook(std::string const &dbName);
BridgeResult sqlite_register_commit_hook(
    std::string const &dbName,
    std::function<void(std::string dbName)> const callback);
BridgeResult sqlite_deregister_commit_hook(std::string const &dbName);
BridgeResult sqlite_register_rollback_hook(
    std::string const &dbName,
    std::function<void(std::string dbName)> const callback);
BridgeResult sqlite_deregister_rollback_hook(std::string const &dbName);

sqlite3_stmt *sqlite_prepare_statement(std::string const &dbName,
                                       std::string const &query);

void sqlite_bind_statement(sqlite3_stmt *statement,
                           const std::vector<JSVariant> *params);

BridgeResult sqlite_execute_prepared_statement(
    std::string const &dbName, sqlite3_stmt *statement,
    std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> metadatas);
} // namespace opsqlite

#endif /* bridge_h */
