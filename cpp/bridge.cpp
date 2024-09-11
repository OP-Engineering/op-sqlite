#include "bridge.h"
#include "DumbHostObject.h"
#include "SmartHostObject.h"
#include "logs.h"
#include "utils.h"
#include <iostream>
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

//            _____ _____
//      /\   |  __ \_   _|
//     /  \  | |__) || |
//    / /\ \ |  ___/ | |
//   / ____ \| |    _| |_
//  /_/    \_\_|   |_____|

/// Returns the completely formed db path, but it also creates any sub-folders
/// along the way
std::string opsqlite_get_db_path(std::string const &db_name,
                                 std::string const &location) {

  if (location == ":memory:") {
    return location;
  }

  mkdir(location);
  return location + "/" + db_name;
}

#ifdef OP_SQLITE_USE_SQLCIPHER
BridgeResult opsqlite_open(std::string const &dbName,
                           std::string const &last_path,
                           std::string const &crsqlite_path,
                           std::string const &sqlite_vec_path,
                           std::string const &encryptionKey) {
#else
BridgeResult opsqlite_open(std::string const &dbName,
                           std::string const &last_path,
                           std::string const &crsqlite_path,
                           std::string const &sqlite_vec_path) {
#endif
  std::string dbPath = opsqlite_get_db_path(dbName, last_path);

  int sqlOpenFlags =
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;

  sqlite3 *db;

  int status = sqlite3_open_v2(dbPath.c_str(), &db, sqlOpenFlags, nullptr);

  if (status != SQLITE_OK) {
    return {.type = SQLiteError, .message = sqlite3_errmsg(db)};
  }

  dbMap[dbName] = db;

#ifdef OP_SQLITE_USE_SQLCIPHER
  opsqlite_execute(dbName, "PRAGMA key = '" + encryptionKey + "'", nullptr);
#endif

  sqlite3_enable_load_extension(db, 1);

  char *errMsg;

#ifdef OP_SQLITE_USE_CRSQLITE
  const char *crsqliteEntryPoint = "sqlite3_crsqlite_init";

  sqlite3_load_extension(db, crsqlite_path.c_str(), crsqliteEntryPoint,
                         &errMsg);

  if (errMsg != nullptr) {
    return {.type = SQLiteError, .message = errMsg};
  } else {
    LOGI("Loaded CRSQlite successfully");
  }
#endif

#ifdef OP_SQLITE_USE_SQLITE_VEC
  const char *vec_entry_point = "sqlite3_vec_init";

  sqlite3_load_extension(db, sqlite_vec_path.c_str(), vec_entry_point, &errMsg);

  if (errMsg != nullptr) {
    return {.type = SQLiteError, .message = errMsg};
  } else {
    LOGI("Loaded sqlite-vec successfully");
  }

#endif

  return {.type = SQLiteOk, .affectedRows = 0};
}

BridgeResult opsqlite_close(std::string const &dbName) {

  check_db_open(dbName);

  sqlite3 *db = dbMap[dbName];

#ifdef OP_SQLITE_USE_CRSQLITE
  opsqlite_execute(dbName, "select crsql_finalize();", nullptr);
#endif

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
  std::string dbPath = opsqlite_get_db_path(databaseToAttach, docPath);
  std::string statement = "ATTACH DATABASE '" + dbPath + "' AS " + alias;

  BridgeResult result = opsqlite_execute(mainDBName, statement, nullptr);

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
  std::string statement = "DETACH DATABASE " + alias;
  BridgeResult result = opsqlite_execute(mainDBName, statement, nullptr);
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

  std::string dbPath = opsqlite_get_db_path(dbName, docPath);

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
  // reset any existing bound values
  sqlite3_clear_bindings(statement);

  size_t size = values->size();

  for (int ii = 0; ii < size; ii++) {
    int sqIndex = ii + 1;
    JSVariant value = values->at(ii);

    if (std::holds_alternative<bool>(value) ||
        std::holds_alternative<int>(value)) {
      sqlite3_bind_int(statement, sqIndex, std::get<int>(value));
    } else if (std::holds_alternative<long long>(value)) {
      sqlite3_bind_double(statement, sqIndex,
                          static_cast<double>(std::get<long long>(value)));
    } else if (std::holds_alternative<double>(value)) {
      sqlite3_bind_double(statement, sqIndex, std::get<double>(value));
    } else if (std::holds_alternative<std::string>(value)) {
      std::string str = std::get<std::string>(value);
      sqlite3_bind_text(statement, sqIndex, str.c_str(),
                        static_cast<int>(str.length()), SQLITE_TRANSIENT);
    } else if (std::holds_alternative<ArrayBuffer>(value)) {
      ArrayBuffer buffer = std::get<ArrayBuffer>(value);
      sqlite3_bind_blob(statement, sqIndex, buffer.data.get(),
                        static_cast<int>(buffer.size), SQLITE_TRANSIENT);
    } else {
      sqlite3_bind_null(statement, sqIndex);
    }
  }
}

BridgeResult opsqlite_execute_prepared_statement(
    std::string const &dbName, sqlite3_stmt *statement,
    std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> &metadatas) {

  check_db_open(dbName);

  sqlite3 *db = dbMap[dbName];

  const char *errorMessage;

  bool isConsuming = true;
  bool isFailed = false;

  int result = SQLITE_OK;

  int i, count, column_type;
  std::string column_name, column_declared_type;

  while (isConsuming) {
    result = sqlite3_step(statement);

    switch (result) {
    case SQLITE_ROW: {
      if (results == nullptr) {
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
          row.values.emplace_back(column_value);
          break;
        }

        case SQLITE_FLOAT: {
          double column_value = sqlite3_column_double(statement, i);
          row.values.emplace_back(column_value);
          break;
        }

        case SQLITE_TEXT: {
          const char *column_value =
              reinterpret_cast<const char *>(sqlite3_column_text(statement, i));
          int byteLen = sqlite3_column_bytes(statement, i);
          // Specify length too; in case string contains NULL in the middle
          row.values.emplace_back(std::string(column_value, byteLen));
          break;
        }

        case SQLITE_BLOB: {
          int blob_size = sqlite3_column_bytes(statement, i);
          const void *blob = sqlite3_column_blob(statement, i);
          auto *data = new uint8_t[blob_size];
          // You cannot share raw memory between native and JS
          // always copy the data
          memcpy(data, blob, blob_size);
          row.values.emplace_back(
              ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                          .size = static_cast<size_t>(blob_size)});
          break;
        }

        case SQLITE_NULL:
          // Intentionally left blank

        default:
          row.values.emplace_back(nullptr);
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
          metadata.fields.emplace_back("name", column_name);
          metadata.fields.emplace_back("index", i);
          metadata.fields.emplace_back("type",
                                       type == nullptr ? "UNKNOWN" : type);

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

  sqlite3_reset(statement);

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

  int statementStatus =
      sqlite3_prepare_v2(db, queryStr, -1, &statement, nullptr);

  if (statementStatus == SQLITE_ERROR) {
    const char *message = sqlite3_errmsg(db);
    throw std::runtime_error("[op-sqlite] SQL prepare statement error: " +
                             std::string(message));
  }

  return statement;
}

BridgeResult opsqlite_execute(std::string const &name, std::string const &query,
                              const std::vector<JSVariant> *params) {
  check_db_open(name);

  sqlite3 *db = dbMap[name];

  sqlite3_stmt *statement;
  const char *errorMessage;
  const char *remainingStatement = nullptr;

  bool isFailed = false;
  int step_result, current_column, column_count, column_type;
  std::string column_name, column_declared_type;
  std::vector<std::string> column_names;
  std::vector<std::vector<JSVariant>> rows;
  std::vector<JSVariant> row;

  do {
    const char *queryStr =
        remainingStatement == nullptr ? query.c_str() : remainingStatement;

    int statementStatus =
        sqlite3_prepare_v2(db, queryStr, -1, &statement, &remainingStatement);

    if (statementStatus != SQLITE_OK) {
      errorMessage = sqlite3_errmsg(db);
      return {.type = SQLiteError,
              .message =
                  "[op-sqlite] SQL prepare error: " + std::string(errorMessage),
              .affectedRows = 0};
    }

    if (params != nullptr && !params->empty()) {
      opsqlite_bind_statement(statement, params);
    }

    column_count = sqlite3_column_count(statement);
    bool is_consuming = true;
    // Do a first pass to get the column names
    for (int i = 0; i < column_count; i++) {
      column_name = sqlite3_column_name(statement, i);
      column_names.push_back(column_name);
    }

    double double_value;
    const char *string_value;
    while (is_consuming) {
      step_result = sqlite3_step(statement);

      switch (step_result) {
      case SQLITE_ROW:
        current_column = 0;
        row = std::vector<JSVariant>();
        column_count = sqlite3_column_count(statement);

        while (current_column < column_count) {
          column_type = sqlite3_column_type(statement, current_column);

          switch (column_type) {

          case SQLITE_INTEGER:
            // intentional fallthrough
          case SQLITE_FLOAT: {
            double_value = sqlite3_column_double(statement, current_column);
            row.emplace_back(double_value);
            break;
          }

          case SQLITE_TEXT: {
            string_value = reinterpret_cast<const char *>(
                sqlite3_column_text(statement, current_column));
            int byteLen = sqlite3_column_bytes(statement, current_column);
            // Specify length too; in case string contains NULL in the middle
            row.emplace_back(std::string(string_value, byteLen));
            break;
          }

          case SQLITE_BLOB: {
            int blob_size = sqlite3_column_bytes(statement, current_column);
            const void *blob = sqlite3_column_blob(statement, current_column);
            auto *data = new uint8_t[blob_size];
            memcpy(data, blob, blob_size);
            row.emplace_back(
                ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                            .size = static_cast<size_t>(blob_size)});
            break;
          }

          case SQLITE_NULL:
            // Intentionally left blank to switch to default case
          default:
            row.emplace_back(nullptr);
            break;
          }

          current_column++;
        }

        rows.push_back(row);
        break;

      case SQLITE_DONE:
        is_consuming = false;
        break;

      default:
        isFailed = true;
        is_consuming = false;
      }
    }

    sqlite3_finalize(statement);
  } while (remainingStatement != nullptr &&
           strcmp(remainingStatement, "") != 0 && !isFailed);

  if (isFailed) {
    const char *message = sqlite3_errmsg(db);
    return {.type = SQLiteError,
            .message =
                "[op-sqlite] SQL execution error: " + std::string(message),
            .affectedRows = 0,
            .insertId = 0};
  }

  int changedRowCount = sqlite3_changes(db);
  long long latestInsertRowId = sqlite3_last_insert_rowid(db);
  return {.type = SQLiteOk,
          .affectedRows = changedRowCount,
          .insertId = static_cast<double>(latestInsertRowId),
          .rows = std::move(rows),
          .column_names = std::move(column_names)};
}

/// Base execution function, returns HostObjects to the JS environment
BridgeResult opsqlite_execute_host_objects(
    std::string const &dbName, std::string const &query,
    const std::vector<JSVariant> *params, std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> &metadatas) {

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
      return {.type = SQLiteError,
              .message =
                  "[op-sqlite] SQL statement error on opsqlite_execute:\n" +
                  std::to_string(statementStatus) + " description:\n" +
                  std::string(message)};
    }

    // The statement did not fail to parse but there is nothing to do, just
    // skip to the end
    if (statement == nullptr) {
      continue;
    }

    if (params != nullptr && !params->empty()) {
      opsqlite_bind_statement(statement, params);
    }

    int i, count, column_type;
    std::string column_name, column_declared_type;

    while (isConsuming) {
      result = sqlite3_step(statement);

      switch (result) {
      case SQLITE_ROW: {
        if (results == nullptr) {
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
            row.values.emplace_back(column_value);
            break;
          }

          case SQLITE_FLOAT: {
            double column_value = sqlite3_column_double(statement, i);
            row.values.emplace_back(column_value);
            break;
          }

          case SQLITE_TEXT: {
            const char *column_value = reinterpret_cast<const char *>(
                sqlite3_column_text(statement, i));
            int byteLen = sqlite3_column_bytes(statement, i);
            // Specify length too; in case string contains NULL in the middle
            row.values.emplace_back(std::string(column_value, byteLen));
            break;
          }

          case SQLITE_BLOB: {
            int blob_size = sqlite3_column_bytes(statement, i);
            const void *blob = sqlite3_column_blob(statement, i);
            auto *data = new uint8_t[blob_size];
            // You cannot share raw memory between native and JS
            // always copy the data
            memcpy(data, blob, blob_size);
            row.values.emplace_back(
                ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                            .size = static_cast<size_t>(blob_size)});
            break;
          }

          case SQLITE_NULL:
            // Intentionally left blank

          default:
            row.values.emplace_back(nullptr);
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
            metadata.fields.emplace_back("name", column_name);
            metadata.fields.emplace_back("index", i);
            metadata.fields.emplace_back("type",
                                         type == nullptr ? "UNKNOWN" : type);

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
  } while (remainingStatement != nullptr &&
           strcmp(remainingStatement, "") != 0 && !isFailed);

  if (isFailed) {

    return {.type = SQLiteError,
            .message =
                "[op-sqlite] SQLite error code: " + std::to_string(result) +
                ", description: " + std::string(errorMessage)};
  }

  int changedRowCount = sqlite3_changes(db);
  long long latestInsertRowId = sqlite3_last_insert_rowid(db);

  return {.type = SQLiteOk,
          .affectedRows = changedRowCount,
          .insertId = static_cast<double>(latestInsertRowId)};
}

/// Executes returning data in raw arrays, a small performance optimization
/// for certain use cases
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

  int step = SQLITE_OK;

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
                     " description:" + std::string(message),
      };
    }

    // The statement did not fail to parse but there is nothing to do, just
    // skip to the end
    if (statement == nullptr) {
      continue;
    }

    if (params != nullptr && !params->empty()) {
      opsqlite_bind_statement(statement, params);
    }

    int i, count, column_type;
    std::string column_name, column_declared_type;

    while (isConsuming) {
      step = sqlite3_step(statement);

      switch (step) {
      case SQLITE_ROW: {
        if (results == nullptr) {
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
            row.emplace_back(column_value);
            break;
          }

          case SQLITE_FLOAT: {
            double column_value = sqlite3_column_double(statement, i);
            row.emplace_back(column_value);
            break;
          }

          case SQLITE_TEXT: {
            const char *column_value = reinterpret_cast<const char *>(
                sqlite3_column_text(statement, i));
            int byteLen = sqlite3_column_bytes(statement, i);
            // Specify length too; in case string contains NULL in the middle
            row.emplace_back(std::string(column_value, byteLen));
            break;
          }

          case SQLITE_BLOB: {
            int blob_size = sqlite3_column_bytes(statement, i);
            const void *blob = sqlite3_column_blob(statement, i);
            auto *data = new uint8_t[blob_size];
            memcpy(data, blob, blob_size);
            row.emplace_back(
                ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                            .size = static_cast<size_t>(blob_size)});
            break;
          }

          case SQLITE_NULL:
            // intentional fallthrough
          default:
            row.emplace_back(nullptr);
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
  } while (remainingStatement != nullptr &&
           strcmp(remainingStatement, "") != 0 && !isFailed);

  if (isFailed) {

    return {.type = SQLiteError,
            .message =
                "[op-sqlite] SQLite error code: " + std::to_string(step) +
                ", description: " + std::string(errorMessage)};
  }

  int changedRowCount = sqlite3_changes(db);
  long long latestInsertRowId = sqlite3_last_insert_rowid(db);

  return {.type = SQLiteOk,
          .affectedRows = changedRowCount,
          .insertId = static_cast<double>(latestInsertRowId)};
}

void opsqlite_close_all() {
  for (auto const &x : dbMap) {
    // Interrupt will make all pending operations to fail with
    // SQLITE_INTERRUPT The ongoing work from threads will then fail ASAP
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

  sqlite3_update_hook(db, nullptr, nullptr);

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
  sqlite3_commit_hook(db, nullptr, nullptr);

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

  sqlite3_rollback_hook(db, nullptr, nullptr);

  return {SQLiteOk};
}

BridgeResult opsqlite_load_extension(std::string const &db_name,
                                     std::string &path,
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

BatchResult opsqlite_execute_batch(std::string &name,
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
    opsqlite_execute(name, "BEGIN EXCLUSIVE TRANSACTION", nullptr);
    for (int i = 0; i < commandCount; i++) {
      auto command = commands->at(i);
      // We do not provide a datastructure to receive query data because we
      // don't need/want to handle this results in a batch execution
      auto result = opsqlite_execute(name, command.sql, command.params.get());
      if (result.type == SQLiteError) {
        opsqlite_execute(name, "ROLLBACK", nullptr);
        return BatchResult{
            .type = SQLiteError,
            .message = result.message,
        };
      } else {
        affectedRows += result.affectedRows;
      }
    }
    opsqlite_execute(name, "COMMIT", nullptr);
    return BatchResult{
        .type = SQLiteOk,
        .affectedRows = affectedRows,
        .commands = static_cast<int>(commandCount),
    };
  } catch (std::exception &exc) {
    opsqlite_execute(name, "ROLLBACK", nullptr);
    return BatchResult{
        .type = SQLiteError,
        .message = exc.what(),
    };
  }
}

} // namespace opsqlite
