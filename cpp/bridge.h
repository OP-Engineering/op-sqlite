#ifndef bridge_h
#define bridge_h

#include "utils.h"
#include <vector>
#include "DynamicHostObject.h"
#include "DumbHostObject.h"
#include "types.h"

namespace opsqlite {

namespace jsi = facebook::jsi;

BridgeResult sqliteOpenDb(std::string const dbName, std::string const docPath, bool memoryStorage);

BridgeResult sqliteCloseDb(std::string const dbName);

BridgeResult sqliteRemoveDb(std::string const dbName, std::string const docPath);

BridgeResult sqliteAttachDb(std::string const mainDBName, std::string const docPath, std::string const databaseToAttach, std::string const alias);

BridgeResult sqliteDetachDb(std::string const mainDBName, std::string const alias);

BridgeResult sqliteExecute(std::string const dbName,
                           std::string const &query,
                           std::vector<JSVariant> *values,
                           std::vector<DumbHostObject> *results,
                           std::shared_ptr<std::vector<DynamicHostObject>> metadatas);

BridgeResult sqliteExecuteLiteral(std::string const dbName, std::string const &query);

void sqliteCloseAll();

}

#endif /* bridge_h */
