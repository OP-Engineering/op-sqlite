#include "bridge.h"
#include "DumbHostObject.h"
#include "SmartHostObject.h"
#include "logs.h"
#include "utils.h"
#include <ctime>
#include <unordered_map>
#include <variant>

namespace opsqlite {

std::unordered_map<std::string, sqlite3 *> dbMap =
    std::unordered_map<std::string, sqlite3 *>();
std::unordered_map<std::string,
                   std::function<void(std::string dbName, std::string tableName,
                                      std::string operation, int rowId)>>
    updateCallbackMap = std::unordered_map<
        std::string,
        std::function<void(std::string dbName, std::string tableName,
                           std::string operation, int rowId)>>();

std::unordered_map<std::string, std::function<void(std::string dbName)>>
    commitCallbackMap =
        std::unordered_map<std::string,
                           std::function<void(std::string dbName)>>();

std::unordered_map<std::string, std::function<void(std::string dbName)>>
    rollbackCallbackMap =
        std::unordered_map<std::string,
                           std::function<void(std::string dbName)>>();

std::string get_db_path(std::string const dbName, std::string const lastPath) {
  if (lastPath == ":memory:") {
    return lastPath;
  }
  mkdir(lastPath);
  return lastPath + "/" + dbName;
}

BridgeResult sqlite_open(std::string const &dbName,
                         std::string const &lastPath) {
  std::string dbPath = get_db_path(dbName, lastPath);

  int sqlOpenFlags =
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;

  sqlite3 *db;
  int exit = 0;
  exit = sqlite3_open_v2(dbPath.c_str(), &db, sqlOpenFlags, nullptr);

  if (exit != SQLITE_OK) {
    return {.type = SQLiteError, .message = sqlite3_errmsg(db)};
  }

  dbMap[dbName] = db;

  return BridgeResult{.type = SQLiteOk, .affectedRows = 0};
}

BridgeResult sqlite_close(std::string const &dbName) {

  if (dbMap.count(dbName) == 0) {
    return {
        .type = SQLiteError,
        .message = dbName + " is not open",
    };
  }

  sqlite3 *db = dbMap[dbName];

  sqlite3_close_v2(db);

  dbMap.erase(dbName);

  return BridgeResult{
      .type = SQLiteOk,
  };
}

BridgeResult sqlite_attach(std::string const &mainDBName,
                           std::string const &docPath,
                           std::string const &databaseToAttach,
                           std::string const &alias) {
  /**
   * There is no need to check if mainDBName is opened because
   * sqliteExecuteLiteral will do that.
   * */
  std::string dbPath = get_db_path(databaseToAttach, docPath);
  std::string statement = "ATTACH DATABASE '" + dbPath + "' AS " + alias;

  BridgeResult result = sqlite_execute_literal(mainDBName, statement);

  if (result.type == SQLiteError) {
    return {
        .type = SQLiteError,
        .message = mainDBName + " was unable to attach another database: " +
                   std::string(result.message),
    };
  }
  return {
      .type = SQLiteOk,
  };
}

BridgeResult sqlite_detach(std::string const &mainDBName,
                           std::string const &alias) {
  /**
   * There is no need to check if mainDBName is opened because
   * sqliteExecuteLiteral will do that.
   * */
  std::string statement = "DETACH DATABASE " + alias;
  BridgeResult result = sqlite_execute_literal(mainDBName, statement);
  if (result.type == SQLiteError) {
    return BridgeResult{
        .type = SQLiteError,
        .message = mainDBName + "was unable to detach database: " +
                   std::string(result.message),
    };
  }
  return BridgeResult{
      .type = SQLiteOk,
  };
}

BridgeResult sqlite_remove(std::string const &dbName,
                           std::string const &docPath) {
  if (dbMap.count(dbName) == 1) {
    BridgeResult closeResult = sqlite_close(dbName);
    if (closeResult.type == SQLiteError) {
      return closeResult;
    }
  }

  std::string dbPath = get_db_path(dbName, docPath);

  if (!file_exists(dbPath)) {
    return {.type = SQLiteError,
            .message = "[op-sqlite]: Database file not found" + dbPath};
  }

  remove(dbPath.c_str());

  return {
      .type = SQLiteOk,
  };
}

inline void bindStatement(sqlite3_stmt *statement,
                          const std::vector<JSVariant> *values) {
  size_t size = values->size();

  for (int ii = 0; ii < size; ii++) {
    int sqIndex = ii + 1;
    JSVariant value = values->at(ii);

    if (std::holds_alternative<bool>(value)) {
      sqlite3_bind_int(statement, sqIndex, std::get<bool>(value));
    } else if (std::holds_alternative<int>(value)) {
      sqlite3_bind_int(statement, sqIndex, std::get<int>(value));
    } else if (std::holds_alternative<long long>(value)) {
      sqlite3_bind_double(statement, sqIndex, std::get<long long>(value));
    } else if (std::holds_alternative<double>(value)) {
      sqlite3_bind_double(statement, sqIndex, std::get<double>(value));
    } else if (std::holds_alternative<std::string>(value)) {
      std::string str = std::get<std::string>(value);
      sqlite3_bind_text(statement, sqIndex, str.c_str(), str.length(),
                        SQLITE_TRANSIENT);
    } else if (std::holds_alternative<ArrayBuffer>(value)) {
      ArrayBuffer buffer = std::get<ArrayBuffer>(value);
      sqlite3_bind_blob(statement, sqIndex, buffer.data.get(), buffer.size,
                        SQLITE_STATIC);
    } else {
      sqlite3_bind_null(statement, sqIndex);
    }
  }
}

void sqlite_bind_statement(sqlite3_stmt *statement,
                           const std::vector<JSVariant> *params) {
  bindStatement(statement, params);
}

BridgeResult sqlite_execute_prepared_statement(
    std::string const &dbName, sqlite3_stmt *statement,
    std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> metadatas) {
  if (dbMap.find(dbName) == dbMap.end()) {
    return {.type = SQLiteError,
            .message = "[op-sqlite]: Database " + dbName + " is not open"};
  }

  sqlite3_reset(statement);

  sqlite3 *db = dbMap[dbName];

  const char *errorMessage;

  bool isConsuming = true;
  bool isFailed = false;

  int result = SQLITE_OK;

  isConsuming = true;

  int i, count, column_type;
  std::string column_name, column_declared_type;

  while (isConsuming) {
    result = sqlite3_step(statement);

    switch (result) {
    case SQLITE_ROW: {
      if (results == NULL) {
        break;
      }

      i = 0;
      DumbHostObject row = DumbHostObject(metadatas);

      count = sqlite3_column_count(statement);

      while (i < count) {
        column_type = sqlite3_column_type(statement, i);

        switch (column_type) {
        case SQLITE_INTEGER: {
          /**
           * Warning this will loose precision because JS can
           * only represent Integers up to 53 bits
           */
          double column_value = sqlite3_column_double(statement, i);
          row.values.push_back(JSVariant(column_value));
          break;
        }

        case SQLITE_FLOAT: {
          double column_value = sqlite3_column_double(statement, i);
          row.values.push_back(JSVariant(column_value));
          break;
        }

        case SQLITE_TEXT: {
          const char *column_value =
              reinterpret_cast<const char *>(sqlite3_column_text(statement, i));
          int byteLen = sqlite3_column_bytes(statement, i);
          // Specify length too; in case string contains NULL in the middle
          row.values.push_back(JSVariant(std::string(column_value, byteLen)));
          break;
        }

        case SQLITE_BLOB: {
          int blob_size = sqlite3_column_bytes(statement, i);
          const void *blob = sqlite3_column_blob(statement, i);
          uint8_t *data = new uint8_t[blob_size];
          // You cannot share raw memory between native and JS
          // always copy the data
          memcpy(data, blob, blob_size);
          row.values.push_back(
              JSVariant(ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                                    .size = static_cast<size_t>(blob_size)}));
          break;
        }

        case SQLITE_NULL:
          // Intentionally left blank

        default:
          row.values.push_back(JSVariant(NULL));
          break;
        }
        i++;
      }
      if (results != nullptr) {
        results->push_back(row);
      }
      break;
    }

    case SQLITE_DONE:
      if (metadatas != nullptr) {
        i = 0;
        count = sqlite3_column_count(statement);

        while (i < count) {
          column_name = sqlite3_column_name(statement, i);
          const char *type = sqlite3_column_decltype(statement, i);
          auto metadata = SmartHostObject();
          metadata.fields.push_back(std::make_pair("name", column_name));
          metadata.fields.push_back(std::make_pair("index", i));
          metadata.fields.push_back(
              std::make_pair("type", type == NULL ? "UNKNOWN" : type));

          metadatas->push_back(metadata);
          i++;
        }
      }
      isConsuming = false;
      break;

    default:
      errorMessage = sqlite3_errmsg(db);
      isFailed = true;
      isConsuming = false;
    }
  }

  if (isFailed) {

    return {.type = SQLiteError,
            .message = "[op-sqlite] SQLite code: " + std::to_string(result) +
                       " execution error: " + std::string(errorMessage)};
  }

  int changedRowCount = sqlite3_changes(db);
  long long latestInsertRowId = sqlite3_last_insert_rowid(db);

  return {.type = SQLiteOk,
          .affectedRows = changedRowCount,
          .insertId = static_cast<double>(latestInsertRowId)};
}

sqlite3_stmt *sqlite_prepare_statement(std::string const &dbName,
                                       std::string const &query) {
  if (dbMap.find(dbName) == dbMap.end()) {
    throw std::runtime_error("Database not opened");
  }

  sqlite3 *db = dbMap[dbName];

  sqlite3_stmt *statement;

  const char *queryStr = query.c_str();

  int statementStatus = sqlite3_prepare_v2(db, queryStr, -1, &statement, NULL);

  if (statementStatus == SQLITE_ERROR) {
    const char *message = sqlite3_errmsg(db);
    throw std::runtime_error("[op-sqlite] SQL statement error: " +
                             std::string(message));
  }

  return statement;
}

BridgeResult
sqlite_execute(std::string const &dbName, std::string const &query,
               const std::vector<JSVariant> *params,
               std::vector<DumbHostObject> *results,
               std::shared_ptr<std::vector<SmartHostObject>> metadatas) {

  if (dbMap.find(dbName) == dbMap.end()) {
    return {.type = SQLiteError,
            .message = "[op-sqlite]: Database " + dbName + " is not open"};
  }

  sqlite3 *db = dbMap[dbName];

  sqlite3_stmt *statement;
  const char *errorMessage;
  const char *remainingStatement = nullptr;

  bool isConsuming = true;
  bool isFailed = false;

  int result = SQLITE_OK;

  do {
    const char *queryStr =
        remainingStatement == nullptr ? query.c_str() : remainingStatement;

    int statementStatus =
        sqlite3_prepare_v2(db, queryStr, -1, &statement, &remainingStatement);

    if (statementStatus != SQLITE_OK) {
      const char *message = sqlite3_errmsg(db);
      return {
          .type = SQLiteError,
          .message = "[op-sqlite] SQL statement error:" +
                     std::to_string(statementStatus) +
                     " description:" + std::string(message) +
                     ". See error codes: https://www.sqlite.org/rescode.html",
      };
    }

    // The statement did not fail to parse but there is nothing to do, just skip
    // to the end
    if (statement == NULL) {
      continue;
    }

    if (params != nullptr && params->size() > 0) {
      bindStatement(statement, params);
    }

    isConsuming = true;

    int i, count, column_type;
    std::string column_name, column_declared_type;

    while (isConsuming) {
      result = sqlite3_step(statement);

      switch (result) {
      case SQLITE_ROW: {
        if (results == NULL) {
          break;
        }

        i = 0;
        DumbHostObject row = DumbHostObject(metadatas);

        count = sqlite3_column_count(statement);

        while (i < count) {
          column_type = sqlite3_column_type(statement, i);

          switch (column_type) {
          case SQLITE_INTEGER: {
            /**
             * Warning this will loose precision because JS can
             * only represent Integers up to 53 bits
             */
            double column_value = sqlite3_column_double(statement, i);
            row.values.push_back(JSVariant(column_value));
            break;
          }

          case SQLITE_FLOAT: {
            double column_value = sqlite3_column_double(statement, i);
            row.values.push_back(JSVariant(column_value));
            break;
          }

          case SQLITE_TEXT: {
            const char *column_value = reinterpret_cast<const char *>(
                sqlite3_column_text(statement, i));
            int byteLen = sqlite3_column_bytes(statement, i);
            // Specify length too; in case string contains NULL in the middle
            row.values.push_back(JSVariant(std::string(column_value, byteLen)));
            break;
          }

          case SQLITE_BLOB: {
            int blob_size = sqlite3_column_bytes(statement, i);
            const void *blob = sqlite3_column_blob(statement, i);
            uint8_t *data = new uint8_t[blob_size];
            // You cannot share raw memory between native and JS
            // always copy the data
            memcpy(data, blob, blob_size);
            row.values.push_back(
                JSVariant(ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                                      .size = static_cast<size_t>(blob_size)}));
            break;
          }

          case SQLITE_NULL:
            // Intentionally left blank

          default:
            row.values.push_back(JSVariant(NULL));
            break;
          }
          i++;
        }
        if (results != nullptr) {
          results->push_back(row);
        }
        break;
      }

      case SQLITE_DONE:
        if (metadatas != nullptr) {
          i = 0;
          count = sqlite3_column_count(statement);

          while (i < count) {
            column_name = sqlite3_column_name(statement, i);
            const char *type = sqlite3_column_decltype(statement, i);
            auto metadata = SmartHostObject();
            metadata.fields.push_back(std::make_pair("name", column_name));
            metadata.fields.push_back(std::make_pair("index", i));
            metadata.fields.push_back(
                std::make_pair("type", type == NULL ? "UNKNOWN" : type));

            metadatas->push_back(metadata);
            i++;
          }
        }
        isConsuming = false;
        break;

      default:
        errorMessage = sqlite3_errmsg(db);
        isFailed = true;
        isConsuming = false;
      }
    }

    sqlite3_finalize(statement);
  } while (remainingStatement != NULL && strcmp(remainingStatement, "") != 0 &&
           !isFailed);

  if (isFailed) {

    return {.type = SQLiteError,
            .message =
                "[op-sqlite] SQLite error code: " + std::to_string(result) +
                ", description: " + std::string(errorMessage) +
                ".\nSee SQLite error codes reference: "
                "https://www.sqlite.org/rescode.html"};
  }

  int changedRowCount = sqlite3_changes(db);
  long long latestInsertRowId = sqlite3_last_insert_rowid(db);

  return {.type = SQLiteOk,
          .affectedRows = changedRowCount,
          .insertId = static_cast<double>(latestInsertRowId)};
}

BridgeResult sqlite_execute_literal(std::string const &dbName,
                                    std::string const &query) {
  if (dbMap.count(dbName) == 0) {
    return {SQLiteError, "[op-sqlite] Database not opened: " + dbName};
  }

  sqlite3 *db = dbMap[dbName];
  sqlite3_stmt *statement;

  int statementStatus =
      sqlite3_prepare_v2(db, query.c_str(), -1, &statement, NULL);

  if (statementStatus != SQLITE_OK) {
    const char *message = sqlite3_errmsg(db);
    return {SQLiteError,
            "[op-sqlite] SQL statement error: " + std::string(message), 0};
  }

  bool isConsuming = true;
  bool isFailed = false;

  int result;
  std::string column_name;

  while (isConsuming) {
    result = sqlite3_step(statement);

    switch (result) {
    case SQLITE_ROW:
      isConsuming = true;
      break;

    case SQLITE_DONE:
      isConsuming = false;
      break;

    default:
      isFailed = true;
      isConsuming = false;
    }
  }

  sqlite3_finalize(statement);

  if (isFailed) {
    const char *message = sqlite3_errmsg(db);
    return {SQLiteError,
            "[op-sqlite] SQL execution error: " + std::string(message), 0};
  }

  int changedRowCount = sqlite3_changes(db);
  return {SQLiteOk, "", changedRowCount};
}

void sqlite_close_all() {
  for (auto const &x : dbMap) {
    // Interrupt will make all pending operations to fail with SQLITE_INTERRUPT
    // The ongoing work from threads will then fail ASAP
    sqlite3_interrupt(x.second);
    // Each DB connection can then be safely interrupted
    sqlite3_close_v2(x.second);
  }
  dbMap.clear();
}

std::string operation_to_string(int operation_type) {
  switch (operation_type) {
  case SQLITE_INSERT:
    return "INSERT";

  case SQLITE_DELETE:
    return "DELETE";

  case SQLITE_UPDATE:
    return "UPDATE";

  default:
    throw std::invalid_argument("Uknown SQLite operation on hook");
  }
}

void update_callback(void *dbName, int operation_type, char const *database,
                     char const *table, sqlite3_int64 rowid) {
  std::string &strDbName = *(static_cast<std::string *>(dbName));
  auto callback = updateCallbackMap[strDbName];
  callback(strDbName, std::string(table), operation_to_string(operation_type),
           static_cast<int>(rowid));
}

BridgeResult sqlite_register_update_hook(
    std::string const &dbName,
    std::function<void(std::string dbName, std::string tableName,
                       std::string operation, int rowId)> const callback) {
  if (dbMap.count(dbName) == 0) {
    return {SQLiteError, "[op-sqlite] Database not opened: " + dbName};
  }

  sqlite3 *db = dbMap[dbName];
  updateCallbackMap[dbName] = callback;
  const std::string *key = nullptr;

  // TODO find a more elegant way to retrieve a reference to the key
  for (auto const &element : dbMap) {
    if (element.first == dbName) {
      key = &element.first;
    }
  }

  sqlite3_update_hook(db, &update_callback, (void *)key);

  return {SQLiteOk};
}

BridgeResult sqlite_deregister_update_hook(std::string const &dbName) {
  if (dbMap.count(dbName) == 0) {
    return {SQLiteError, "[op-sqlite] Database not opened: " + dbName};
  }

  sqlite3 *db = dbMap[dbName];
  updateCallbackMap.erase(dbName);

  sqlite3_update_hook(db, NULL, NULL);

  return {SQLiteOk};
}

int commit_callback(void *dbName) {
  std::string &strDbName = *(static_cast<std::string *>(dbName));
  auto callback = commitCallbackMap[strDbName];
  callback(strDbName);
  // You need to return 0 to allow commits to continue
  return 0;
}

BridgeResult sqlite_register_commit_hook(
    std::string const &dbName,
    std::function<void(std::string dbName)> const callback) {
  if (dbMap.count(dbName) == 0) {
    return {SQLiteError, "[op-sqlite] Database not opened: " + dbName};
  }

  sqlite3 *db = dbMap[dbName];
  commitCallbackMap[dbName] = callback;
  const std::string *key = nullptr;

  // TODO find a more elegant way to retrieve a reference to the key
  for (auto const &element : dbMap) {
    if (element.first == dbName) {
      key = &element.first;
    }
  }

  sqlite3_commit_hook(db, &commit_callback, (void *)key);

  return {SQLiteOk};
}

BridgeResult sqlite_deregister_commit_hook(std::string const &dbName) {
  if (dbMap.count(dbName) == 0) {
    return {SQLiteError, "[op-sqlite] Database not opened: " + dbName};
  }

  sqlite3 *db = dbMap[dbName];
  commitCallbackMap.erase(dbName);
  sqlite3_commit_hook(db, NULL, NULL);

  return {SQLiteOk};
}

void rollback_callback(void *dbName) {
  std::string &strDbName = *(static_cast<std::string *>(dbName));
  auto callback = rollbackCallbackMap[strDbName];
  callback(strDbName);
}

BridgeResult sqlite_register_rollback_hook(
    std::string const &dbName,
    std::function<void(std::string dbName)> const callback) {
  if (dbMap.count(dbName) == 0) {
    return {SQLiteError, "[op-sqlite] Database not opened: " + dbName};
  }

  sqlite3 *db = dbMap[dbName];
  rollbackCallbackMap[dbName] = callback;
  const std::string *key = nullptr;

  // TODO find a more elegant way to retrieve a reference to the key
  for (auto const &element : dbMap) {
    if (element.first == dbName) {
      key = &element.first;
    }
  }

  sqlite3_rollback_hook(db, &rollback_callback, (void *)key);

  return {SQLiteOk};
}

BridgeResult sqlite_deregister_rollback_hook(std::string const &dbName) {
  if (dbMap.count(dbName) == 0) {
    return {SQLiteError, "[op-sqlite] Database not opened: " + dbName};
  }

  sqlite3 *db = dbMap[dbName];
  rollbackCallbackMap.erase(dbName);

  sqlite3_rollback_hook(db, NULL, NULL);

  return {SQLiteOk};
}
} // namespace opsqlite
