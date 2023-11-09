/**
 * SQL Batch execution implementation using default sqliteBridge implementation
*/
#include "utils.h"
#include "bridge.h"
#include "types.h"

namespace opsqlite {

namespace jsi = facebook::jsi;

struct BatchArguments {
  std::string sql;
  std::shared_ptr<std::vector<JSVariant>> params;
};

/**
 * Local Helper method to translate JSI objects BatchArguments data structure
 * MUST be called in the JavaScript Thread
*/
void toBatchArguments(jsi::Runtime &rt, jsi::Array const &batchParams, std::vector<BatchArguments> *commands);

/**
 * Execute a batch of commands in a exclusive transaction
*/
BatchResult sqliteExecuteBatch(std::string dbName, std::vector<BatchArguments> *commands);

}

