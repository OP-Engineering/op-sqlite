#ifndef bridge_h
#define bridge_h

#include "DumbHostObject.h"
#include "SmartHostObject.h"
#include "sqlite3.h"
#include "types.h"
#include "utils.h"
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;

/// Convenience types to avoid super long types
typedef std::function<void(std::string dbName, std::string tableName,
                           std::string operation, int rowId)>
    UpdateCallback;
typedef std::function<void(std::string dbName)> CommitCallback;
typedef std::function<void(std::string dbName)> RollbackCallback;

BridgeResult opsqlite_open(std::string const &dbName,
                           std::string const &dbPath);

BridgeResult opsqlite_close(std::string const &dbName);

BridgeResult opsqlite_remove(std::string const &dbName,
                             std::string const &docPath);

BridgeResult opsqlite_attach(std::string const &mainDBName,
                             std::string const &docPath,
                             std::string const &databaseToAttach,
                             std::string const &alias);

BridgeResult opsqlite_detach(std::string const &mainDBName,
                             std::string const &alias);

BridgeResult
opsqlite_execute(std::string const &dbName, std::string const &query,
                 const std::vector<JSVariant> *params,
                 std::vector<DumbHostObject> *results,
                 std::shared_ptr<std::vector<SmartHostObject>> metadatas);

BridgeResult opsqlite_execute_raw(std::string const &dbName,
                                  std::string const &query,
                                  const std::vector<JSVariant> *params,
                                  std::vector<std::vector<JSVariant>> *results);

BridgeResult opsqlite_execute_literal(std::string const &dbName,
                                      std::string const &query);

void opsqlite_close_all();

BridgeResult opsqlite_register_update_hook(std::string const &dbName,
                                           UpdateCallback const callback);
BridgeResult opsqlite_deregister_update_hook(std::string const &dbName);
BridgeResult opsqlite_register_commit_hook(std::string const &dbName,
                                           CommitCallback const callback);
BridgeResult opsqlite_deregister_commit_hook(std::string const &dbName);
BridgeResult opsqlite_register_rollback_hook(std::string const &dbName,
                                             RollbackCallback const callback);
BridgeResult opsqlite_deregister_rollback_hook(std::string const &dbName);

sqlite3_stmt *opsqlite_prepare_statement(std::string const &dbName,
                                         std::string const &query);

void opsqlite_bind_statement(sqlite3_stmt *statement,
                             const std::vector<JSVariant> *params);

BridgeResult opsqlite_execute_prepared_statement(
    std::string const &dbName, sqlite3_stmt *statement,
    std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> metadatas);

BridgeResult opsqlite_load_extension(std::string &db_name, std::string &path,
                                     std::string &entry_point);

} // namespace opsqlite

#endif /* bridge_h */
