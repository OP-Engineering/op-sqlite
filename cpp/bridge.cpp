#include "bridge.h"
#include "DumbHostObject.h"
#include "SmartHostObject.h"
#include "logs.h"
#include "utils.h"
#include <unordered_map>
#include <variant>

namespace opsqlite {

/// Maps to hold the different objects
std::unordered_map<std::string, sqlite3 *> dbMap =
    std::unordered_map<std::string, sqlite3 *>();
std::unordered_map<std::string, UpdateCallback> updateCallbackMap =
    std::unordered_map<std::string, UpdateCallback>();

std::unordered_map<std::string, CommitCallback> commitCallbackMap =
    std::unordered_map<std::string, CommitCallback>();

std::unordered_map<std::string, RollbackCallback> rollbackCallbackMap =
    std::unordered_map<std::string, RollbackCallback>();

inline void check_db_open(std::string const &db_name) {
  if (dbMap.count(db_name) == 0) {
    throw std::runtime_error("[OP-SQLite] DB is not open");
  }
}

/// Start of api

/// Returns the completely formed db path, but it also creates any sub-folders
/// along the way
std::string get_db_path(std::string const &db_name,
                        std::string const &location) {

  if (location == ":memory:") {
    return location;
  }

  mkdir(location);
  return location + "/" + db_name;
}

BridgeResult opsqlite_open(std::string const &dbName,
                           std::string const &lastPath) {
  std::string dbPath = get_db_path(dbName, lastPath);

  int sqlOpenFlags =
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;

  sqlite3 *db;

  int status = sqlite3_open_v2(dbPath.c_str(), &db, sqlOpenFlags, nullptr);

  if (status != SQLITE_OK) {
    return {.type = SQLiteError, .message = sqlite3_errmsg(db)};
  }

  dbMap[dbName] = db;

  return BridgeResult{.type = SQLiteOk, .affectedRows = 0};
}

BridgeResult opsqlite_close(std::string const &dbName) {

  check_db_open(dbName);

  sqlite3 *db = dbMap[dbName];

  sqlite3_close_v2(db);

  dbMap.erase(dbName);

  return BridgeResult{
      .type = SQLiteOk,
  };
}

BridgeResult opsqlite_attach(std::string const &mainDBName,
                             std::string const &docPath,
                             std::string const &databaseToAttach,
                             std::string const &alias) {
  /**
   * There is no need to check if mainDBName is opened because
   * sqliteExecuteLiteral will do that.
   * */
  std::string dbPath = get_db_path(databaseToAttach, docPath);
  std::string statement = "ATTACH DATABASE '" + dbPath + "' AS " + alias;

  BridgeResult result = opsqlite_execute_literal(mainDBName, statement);

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

BridgeResult opsqlite_detach(std::string const &mainDBName,
                             std::string const &alias) {
  /**
   * There is no need to check if mainDBName is opened because
   * sqliteExecuteLiteral will do that.
   * */
  std::string statement = "DETACH DATABASE " + alias;
  BridgeResult result = opsqlite_execute_literal(mainDBName, statement);
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

BridgeResult opsqlite_remove(std::string const &dbName,
                             std::string const &docPath) {
  if (dbMap.count(dbName) == 1) {
    BridgeResult closeResult = opsqlite_close(dbName);
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

inline void opsqlite_bind_statement(sqlite3_stmt *statement,
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

BridgeResult opsqlite_execute_prepared_statement(
    std::string const &dbName, sqlite3_stmt *statement,
    std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> metadatas) {

  check_db_open(dbName);

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
          row.values.push_back(JSVariant(nullptr));
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

sqlite3_stmt *opsqlite_prepare_statement(std::string const &dbName,
                                         std::string const &query) {
  check_db_open(dbName);

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

/// Base execution function, returns HostObjects to the JS environment
BridgeResult
opsqlite_execute(std::string const &dbName, std::string const &query,
                 const std::vector<JSVariant> *params,
                 std::vector<DumbHostObject> *results,
                 std::shared_ptr<std::vector<SmartHostObject>> metadatas) {

  check_db_open(dbName);

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
      opsqlite_bind_statement(statement, params);
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
            row.values.push_back(JSVariant(nullptr));
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

/// Executes returning data in raw arrays, a small performance optimization for
/// certain use cases
BridgeResult
opsqlite_execute_raw(std::string const &dbName, std::string const &query,
                     const std::vector<JSVariant> *params,
                     std::vector<std::vector<JSVariant>> *results) {

  check_db_open(dbName);

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
      opsqlite_bind_statement(statement, params);
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
        std::vector<JSVariant> row;

        i = 0;

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
            row.push_back(JSVariant(column_value));
            break;
          }

          case SQLITE_FLOAT: {
            double column_value = sqlite3_column_double(statement, i);
            row.push_back(JSVariant(column_value));
            break;
          }

          case SQLITE_TEXT: {
            const char *column_value = reinterpret_cast<const char *>(
                sqlite3_column_text(statement, i));
            int byteLen = sqlite3_column_bytes(statement, i);
            // Specify length too; in case string contains NULL in the middle
            row.push_back(JSVariant(std::string(column_value, byteLen)));
            break;
          }

          case SQLITE_BLOB: {
            int blob_size = sqlite3_column_bytes(statement, i);
            const void *blob = sqlite3_column_blob(statement, i);
            uint8_t *data = new uint8_t[blob_size];
            memcpy(data, blob, blob_size);
            row.push_back(
                JSVariant(ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                                      .size = static_cast<size_t>(blob_size)}));
            break;
          }

          case SQLITE_NULL:
            // Intentionally left blank

          default:
            row.push_back(JSVariant(nullptr));
            break;
          }
          i++;
        }

        results->push_back(row);

        break;
      }

      case SQLITE_DONE:
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

/// Executes without returning any results, Useful for performance critical
/// operations
BridgeResult opsqlite_execute_literal(std::string const &dbName,
                                      std::string const &query) {
  check_db_open(dbName);

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

void opsqlite_close_all() {
  for (auto const &x : dbMap) {
    // Interrupt will make all pending operations to fail with SQLITE_INTERRUPT
    // The ongoing work from threads will then fail ASAP
    sqlite3_interrupt(x.second);
    // Each DB connection can then be safely interrupted
    sqlite3_close_v2(x.second);
  }
  dbMap.clear();
  updateCallbackMap.clear();
  rollbackCallbackMap.clear();
  commitCallbackMap.clear();
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

BridgeResult opsqlite_register_update_hook(std::string const &dbName,
                                           UpdateCallback const callback) {
  check_db_open(dbName);

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

BridgeResult opsqlite_deregister_update_hook(std::string const &dbName) {
  check_db_open(dbName);

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

BridgeResult opsqlite_register_commit_hook(std::string const &dbName,
                                           CommitCallback const callback) {
  check_db_open(dbName);

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

BridgeResult opsqlite_deregister_commit_hook(std::string const &dbName) {
  check_db_open(dbName);

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

BridgeResult opsqlite_register_rollback_hook(std::string const &dbName,
                                             RollbackCallback const callback) {
  check_db_open(dbName);

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

BridgeResult opsqlite_deregister_rollback_hook(std::string const &dbName) {
  check_db_open(dbName);

  sqlite3 *db = dbMap[dbName];
  rollbackCallbackMap.erase(dbName);

  sqlite3_rollback_hook(db, NULL, NULL);

  return {SQLiteOk};
}

BridgeResult opsqlite_load_extension(std::string &db_name, std::string &path,
                                     std::string &entry_point) {
#ifdef OP_SQLITE_USE_PHONE_VERSION
  throw std::runtime_error(
      "Embedded version of SQLite does not support loading extensions");
#else
  check_db_open(db_name);

  sqlite3 *db = dbMap[db_name];
  int loading_extensions_enabled = sqlite3_enable_load_extension(db, 1);
  if (loading_extensions_enabled != SQLITE_OK) {
    return {SQLiteError, "[op-sqlite] could not enable extension loading"};
  }
  const char *path_cstr = path.c_str();
  const char *entry_point_cstr;
  if (!entry_point.empty()) {
    entry_point_cstr = entry_point.c_str();
  }

  char *error_message;

  int extension_loaded =
      sqlite3_load_extension(db, path_cstr, entry_point_cstr, &error_message);
  if (extension_loaded != SQLITE_OK) {
    return {SQLiteError, std::string(error_message)};
  }
  return {SQLiteOk};
#endif
}

} // namespace opsqlite
