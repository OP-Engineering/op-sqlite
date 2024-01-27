#include "sqlbatchexecutor.h"

namespace opsqlite {

/**
 * Batch execution implementation
 */

void toBatchArguments(jsi::Runtime &rt, jsi::Array const &batchParams,
                      std::vector<BatchArguments> *commands) {
  for (int i = 0; i < batchParams.length(rt); i++) {
    const jsi::Array &command =
        batchParams.getValueAtIndex(rt, i).asObject(rt).asArray(rt);
    if (command.length(rt) == 0) {
      continue;
    }

    const std::string query =
        command.getValueAtIndex(rt, 0).asString(rt).utf8(rt);
    const jsi::Value &commandParams = command.length(rt) > 1
                                          ? command.getValueAtIndex(rt, 1)
                                          : jsi::Value::undefined();
    if (!commandParams.isUndefined() &&
        commandParams.asObject(rt).isArray(rt) &&
        commandParams.asObject(rt).asArray(rt).length(rt) > 0 &&
        commandParams.asObject(rt)
            .asArray(rt)
            .getValueAtIndex(rt, 0)
            .isObject()) {
      // This arguments is an array of arrays, like a batch update of a single
      // sql command.
      const jsi::Array &batchUpdateParams =
          commandParams.asObject(rt).asArray(rt);
      for (int x = 0; x < batchUpdateParams.length(rt); x++) {
        const jsi::Value &p = batchUpdateParams.getValueAtIndex(rt, x);
        auto params =
            std::make_shared<std::vector<JSVariant>>(toVariantVec(rt, p));
        commands->push_back({query, params});
      }
    } else {
      auto params = std::make_shared<std::vector<JSVariant>>(
          toVariantVec(rt, commandParams));
      commands->push_back({query, params});
    }
  }
}

BatchResult sqliteExecuteBatch(std::string dbName,
                               std::vector<BatchArguments> *commands) {
  size_t commandCount = commands->size();
  if (commandCount <= 0) {
    return BatchResult{
        .type = SQLiteError,
        .message = "No SQL commands provided",
    };
  }

  try {
    int affectedRows = 0;
    opsqlite_execute_literal(dbName, "BEGIN EXCLUSIVE TRANSACTION");
    for (int i = 0; i < commandCount; i++) {
      auto command = commands->at(i);
      // We do not provide a datastructure to receive query data because we
      // don't need/want to handle this results in a batch execution
      auto result = opsqlite_execute(dbName, command.sql, command.params.get(),
                                     nullptr, nullptr);
      if (result.type == SQLiteError) {
        opsqlite_execute_literal(dbName, "ROLLBACK");
        return BatchResult{
            .type = SQLiteError,
            .message = result.message,
        };
      } else {
        affectedRows += result.affectedRows;
      }
    }
    opsqlite_execute_literal(dbName, "COMMIT");
    return BatchResult{
        .type = SQLiteOk,
        .affectedRows = affectedRows,
        .commands = static_cast<int>(commandCount),
    };
  } catch (std::exception &exc) {
    opsqlite_execute_literal(dbName, "ROLLBACK");
    return BatchResult{
        .type = SQLiteError,
        .message = exc.what(),
    };
  }
}

} // namespace opsqlite
