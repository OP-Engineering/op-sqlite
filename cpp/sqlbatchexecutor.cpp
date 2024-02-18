#include "sqlbatchexecutor.h"

namespace opsqlite {

/**
 * Batch execution implementation
 */

void toBatchArguments(jsi::Runtime &rt, jsi::Array const &batchParams,
                      std::vector<BatchArguments> *commands) {
  size_t batchLength = batchParams.length(rt);
  for (int i = 0; i < batchLength; i++) {
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
        commandParams.asObject(rt).asArray(rt).getValueAtIndex(rt, 0).isObject())
    {

      /* This arguments is an array of arrays, like a batch update of a single
      * sql command. This structure can be optmized to reuse prepared statements.
      * That is the idea of this method signature. EG:
      * sql | [[]] - Same SQL, Many different binded values.
      */
      const jsi::Array &batchUpdateParams =
          commandParams.asObject(rt).asArray(rt);
      std::vector<std::vector<JSVariant>> stmtParams;
      size_t batchUpdateParamsSize = batchUpdateParams.length(rt);
      for (int x = 0; x < batchUpdateParamsSize; x++) {
        const jsi::Value &p = batchUpdateParams.getValueAtIndex(rt, x);
        auto params = toVariantVec(rt, p);
        stmtParams.push_back(params);
      }
      auto params =
            std::make_shared<BatchParams>(BatchParams(stmtParams));
      commands->push_back({query, params});
    } else {
      auto params = std::make_shared<BatchParams>(BatchParams(toVariantVec(rt, commandParams)));
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

  auto db = opsqlite_get_connection(dbName);

  try {
    int affectedRows = 0;

    opsqlite_execute_literal(db, "BEGIN EXCLUSIVE TRANSACTION");

    for (int i = 0; i < commandCount; i++) {
      auto command = commands->at(i);

      auto params = command.params.get();
      // Checking if 
      if ( std::holds_alternative<std::vector<std::vector<JSVariant>>>(*params) ) {
        auto multiStatementParams = std::get<std::vector<std::vector<JSVariant>>>(*params);
        auto size = multiStatementParams.size();
        auto stmt = opsqlite_prepare_statement(db, command.sql);
        for (int x = 0; x < size; x++) {
          auto jsValues = multiStatementParams.at(x);
          // Here we manually control the statement reset
          if (x > 0) {
            sqlite3_reset(stmt);
          }
          opsqlite_bind_statement(stmt, &jsValues);
          auto result = opsqlite_execute_prepared_statement(db, stmt, nullptr, nullptr);
          if (result.type == SQLiteError) {
            sqlite3_finalize(stmt);
            opsqlite_execute_literal(db, "ROLLBACK");
            return BatchResult{
                .type = SQLiteError,
                .message = result.message,
            };
          } else {
            affectedRows += result.affectedRows;
          }
        }
        sqlite3_finalize(stmt);
      } else {
        auto jsValues = std::get<std::vector<JSVariant>>(*params);
        // We do not provide a datastructure to receive query data because we
        // don't need/want to handle this results in a batch execution
        auto stmt = opsqlite_prepare_statement(db, command.sql);
        opsqlite_bind_statement(stmt, &jsValues);
        auto result = opsqlite_execute_prepared_statement(db, stmt, nullptr, nullptr);
        if (result.type == SQLiteError) {
          sqlite3_finalize(stmt);
          opsqlite_execute_literal(db, "ROLLBACK");
          return BatchResult{
              .type = SQLiteError,
              .message = result.message,
          };
        } else {
          affectedRows += result.affectedRows;
        }
        sqlite3_finalize(stmt);
      }
    }
    opsqlite_execute_literal(db, "COMMIT");
    return BatchResult{
        .type = SQLiteOk,
        .affectedRows = affectedRows,
        .commands = static_cast<int>(commandCount),
    };
  } catch (std::exception &exc) {
    opsqlite_execute_literal(db, "ROLLBACK");
    return BatchResult{
        .type = SQLiteError,
        .message = exc.what(),
    };
  }
}

} // namespace opsqlite
