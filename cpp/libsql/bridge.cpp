#include "bridge.h"
#include "DumbHostObject.h"
#include "SmartHostObject.h"
#include "logs.h"
#include "utils.h"
#include <iostream>
#include <unordered_map>
#include <variant>

namespace opsqlite {

struct DB {
  libsql_database_t db;
  libsql_connection_t c;
};

std::unordered_map<std::string, DB> db_map =
    std::unordered_map<std::string, DB>();

inline void check_db_open(std::string const &db_name) {
  if (db_map.count(db_name) == 0) {
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
                           std::string const &crsqlitePath,
                           std::string const &encryptionKey) {
#else
BridgeResult opsqlite_libsql_open(std::string const &name,
                                  std::string const &last_path) {
#endif
  std::string path = opsqlite_get_db_path(name, last_path);

  int status = 0;
  libsql_database_t db;
  libsql_connection_t c;
  const char *err = NULL;

  status = libsql_open_file(path.c_str(), &db, &err);

  if (status != 0) {
    return {.type = SQLiteError, .message = err};
  }

  status = libsql_connect(db, &c, &err);

  if (status != 0) {
    return {.type = SQLiteError, .message = err};
  }

  db_map[name] = {.db = db, .c = c};

#ifdef OP_SQLITE_USE_SQLCIPHER
  opsqlite_execute(dbName, "PRAGMA key = '" + encryptionKey + "'", nullptr,
                   nullptr, nullptr);
#endif

  return {.type = SQLiteOk, .affectedRows = 0};
}

BridgeResult opsqlite_libsql_open_remote(std::string const &url,
                                         std::string const &auth_token) {
  int status = 0;
  libsql_database_t db;
  libsql_connection_t c;
  const char *err = NULL;

  status = libsql_open_remote(url.c_str(), auth_token.c_str(), &db, &err);

  if (status != 0) {
    return {.type = SQLiteError, .message = err};
  }

  status = libsql_connect(db, &c, &err);

  if (status != 0) {
    return {.type = SQLiteError, .message = err};
  }

  db_map[url] = {.db = db, .c = c};

  return {.type = SQLiteOk, .affectedRows = 0};
}

BridgeResult opsqlite_libsql_close(std::string const &name) {

  check_db_open(name);

  DB db = db_map[name];

  libsql_disconnect(db.c);
  libsql_close(db.db);

  db_map.erase(name);

  return BridgeResult{
      .type = SQLiteOk,
  };
}

void opsqlite_libsql_close_all() {
  //  for (auto const &db : db_map) {
  //      libsql_close();
  //  }
  for (auto const &db : db_map) {
    opsqlite_libsql_close(db.first);
  }
}

BridgeResult opsqlite_libsql_attach(std::string const &mainDBName,
                                    std::string const &docPath,
                                    std::string const &databaseToAttach,
                                    std::string const &alias) {
  std::string dbPath = opsqlite_get_db_path(databaseToAttach, docPath);
  std::string statement = "ATTACH DATABASE '" + dbPath + "' AS " + alias;

  BridgeResult result =
      opsqlite_libsql_execute(mainDBName, statement, nullptr, nullptr, nullptr);

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

BridgeResult opsqlite_libsql_detach(std::string const &mainDBName,
                                    std::string const &alias) {
  std::string statement = "DETACH DATABASE " + alias;
  BridgeResult result =
      opsqlite_libsql_execute(mainDBName, statement, nullptr, nullptr, nullptr);
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

BridgeResult opsqlite_libsql_remove(std::string const &name,
                                    std::string const &path) {
  if (db_map.count(name) == 1) {
    BridgeResult closeResult = opsqlite_libsql_close(name);
    if (closeResult.type == SQLiteError) {
      return closeResult;
    }
  }

  std::string full_path = opsqlite_get_db_path(name, path);

  if (!file_exists(full_path)) {
    return {.type = SQLiteError,
            .message = "[op-sqlite]: Database file not found" + full_path};
  }

  remove(full_path.c_str());

  return {
      .type = SQLiteOk,
  };
}

void opsqlite_libsql_bind_statement(libsql_stmt_t statement,
                                    const std::vector<JSVariant> *values) {
  const char *err;
  size_t size = values->size();

  for (int ii = 0; ii < size; ii++) {
    int index = ii + 1;
    JSVariant value = values->at(ii);

    if (std::holds_alternative<bool>(value)) {
      libsql_bind_int(statement, index, std::get<int>(value), &err);
    } else if (std::holds_alternative<int>(value)) {
      libsql_bind_int(statement, index, std::get<int>(value), &err);
    } else if (std::holds_alternative<long long>(value)) {
      libsql_bind_int(statement, index, std::get<long long>(value), &err);
    } else if (std::holds_alternative<double>(value)) {
      libsql_bind_float(statement, index, std::get<double>(value), &err);
    } else if (std::holds_alternative<std::string>(value)) {
      std::string str = std::get<std::string>(value);
      libsql_bind_string(statement, index, str.c_str(), &err);
    } else if (std::holds_alternative<ArrayBuffer>(value)) {
      ArrayBuffer buffer = std::get<ArrayBuffer>(value);
      libsql_bind_blob(statement, index, buffer.data.get(), buffer.size, &err);
    } else {
      libsql_bind_null(statement, index, &err);
    }
  }
}

BridgeResult opsqlite_libsql_execute_prepared_statement(
    std::string const &name, libsql_stmt_t stmt,
    std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> metadatas) {

  check_db_open(name);

  libsql_connection_t c = db_map[name].c;
  libsql_rows_t rows;
  libsql_row_t row;

  int status = 0;
  const char *err = NULL;

  if (status != 0) {
    return {.type = SQLiteError, .message = err};
  }

  status = libsql_query_stmt(stmt, &rows, &err);

  if (status != 0) {
    return {.type = SQLiteError, .message = err};
  }

  bool metadata_set = false;

  int num_cols = libsql_column_count(rows);
  while ((status = libsql_next_row(rows, &row, &err)) == 0) {

    if (!err && !row) {
      break;
    }

    DumbHostObject row_host_object = DumbHostObject(metadatas);

    for (int col = 0; col < num_cols; col++) {
      int type;

      libsql_column_type(rows, row, col, &type, &err);

      switch (type) {
      case LIBSQL_INT:
        long long int_value;
        status = libsql_get_int(row, col, &int_value, &err);
        row_host_object.values.push_back(JSVariant(int_value));
        break;

      case LIBSQL_FLOAT:
        double float_value;
        status = libsql_get_float(row, col, &float_value, &err);
        row_host_object.values.push_back(JSVariant(float_value));
        break;

      case LIBSQL_TEXT:
        const char *text_value;
        status = libsql_get_string(row, col, &text_value, &err);
        row_host_object.values.push_back(JSVariant(text_value));
        break;

      case LIBSQL_BLOB: {
        blob value_blob;
        libsql_get_blob(row, col, &value_blob, &err);
        uint8_t *data = new uint8_t[value_blob.len];
        // You cannot share raw memory between native and JS
        // always copy the data
        memcpy(data, value_blob.ptr, value_blob.len);
        libsql_free_blob(value_blob);
        row_host_object.values.push_back(JSVariant(
            ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                        .size = static_cast<size_t>(value_blob.len)}));
        break;
      }

      case LIBSQL_NULL:
        // intentional fall-through
      default:
        row_host_object.values.push_back(JSVariant(nullptr));
        break;
      }

      if (status != 0) {
        fprintf(stderr, "%s\n", err);
        throw std::runtime_error("libsql error");
      }

      // On the first interation through the columns, set the metadata
      if (!metadata_set && metadatas != nullptr) {
        const char *col_name;
        status = libsql_column_name(rows, col, &col_name, &err);

        auto metadata = SmartHostObject();
        metadata.fields.push_back(std::make_pair("name", col_name));
        metadata.fields.push_back(std::make_pair("index", col));
        metadata.fields.push_back(std::make_pair("type", "UNKNOWN"));
        //                  metadata.fields.push_back(
        //                      std::make_pair("type", type == -1 ? "UNKNOWN" :
        //                      type));

        metadatas->push_back(metadata);
      }
    }

    if (results != nullptr) {
      results->push_back(row_host_object);
    }

    metadata_set = true;
    err = NULL;
  }

  if (status != 0) {
    fprintf(stderr, "%s\n", err);
  }

  libsql_free_rows(rows);

  int changes = libsql_changes(c);
  long long insert_row_id = libsql_last_insert_rowid(c);

  libsql_reset_stmt(stmt, &err);

  return {.type = SQLiteOk,
          .affectedRows = changes,
          .insertId = static_cast<double>(insert_row_id)};
}

libsql_stmt_t opsqlite_libsql_prepare_statement(std::string const &name,
                                                std::string const &query) {
  check_db_open(name);

  DB db = db_map[name];

  libsql_stmt_t stmt;

  const char *err;

  int status = libsql_prepare(db.c, query.c_str(), &stmt, &err);

  if (status != 0) {
    throw std::runtime_error(err);
  }

  return stmt;
}

/// Base execution function, returns HostObjects to the JS environment
BridgeResult opsqlite_libsql_execute(
    std::string const &name, std::string const &query,
    const std::vector<JSVariant> *params, std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> metadatas) {

  check_db_open(name);

  libsql_connection_t c = db_map[name].c;
  libsql_rows_t rows;
  libsql_row_t row;
  libsql_stmt_t stmt;
  int status = 0;
  const char *err = NULL;

  status = libsql_prepare(c, query.c_str(), &stmt, &err);

  if (status != 0) {
    return {.type = SQLiteError, .message = err};
  }

  if (params != nullptr && params->size() > 0) {
    opsqlite_libsql_bind_statement(stmt, params);
  }

  status = libsql_query_stmt(stmt, &rows, &err);

  if (status != 0) {
    return {.type = SQLiteError, .message = err};
  }

  bool metadata_set = false;

  int num_cols = libsql_column_count(rows);
  while ((status = libsql_next_row(rows, &row, &err)) == 0) {

    if (!err && !row) {
      break;
    }

    DumbHostObject row_host_object = DumbHostObject(metadatas);

    for (int col = 0; col < num_cols; col++) {
      int type;

      libsql_column_type(rows, row, col, &type, &err);

      switch (type) {
      case LIBSQL_INT:
        long long int_value;
        status = libsql_get_int(row, col, &int_value, &err);
        row_host_object.values.push_back(JSVariant(int_value));
        break;

      case LIBSQL_FLOAT:
        double float_value;
        status = libsql_get_float(row, col, &float_value, &err);
        row_host_object.values.push_back(JSVariant(float_value));
        break;

      case LIBSQL_TEXT:
        const char *text_value;
        status = libsql_get_string(row, col, &text_value, &err);
        row_host_object.values.push_back(JSVariant(text_value));
        break;

      case LIBSQL_BLOB: {
        blob value_blob;
        libsql_get_blob(row, col, &value_blob, &err);
        uint8_t *data = new uint8_t[value_blob.len];
        // You cannot share raw memory between native and JS
        // always copy the data
        memcpy(data, value_blob.ptr, value_blob.len);
        libsql_free_blob(value_blob);
        row_host_object.values.push_back(JSVariant(
            ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                        .size = static_cast<size_t>(value_blob.len)}));
        break;
      }

      case LIBSQL_NULL:
        // intentional fall-through
      default:
        row_host_object.values.push_back(JSVariant(nullptr));
        break;
      }

      if (status != 0) {
        fprintf(stderr, "%s\n", err);
        throw std::runtime_error("libsql error");
      }

      // On the first interation through the columns, set the metadata
      if (!metadata_set && metadatas != nullptr) {
        const char *col_name;
        status = libsql_column_name(rows, col, &col_name, &err);

        auto metadata = SmartHostObject();
        metadata.fields.push_back(std::make_pair("name", col_name));
        metadata.fields.push_back(std::make_pair("index", col));
        metadata.fields.push_back(std::make_pair("type", "UNKNOWN"));
        //                  metadata.fields.push_back(
        //                      std::make_pair("type", type == -1 ? "UNKNOWN" :
        //                      type));

        metadatas->push_back(metadata);
      }
    }

    if (results != nullptr) {
      results->push_back(row_host_object);
    }

    metadata_set = true;
    err = NULL;
  }

  if (status != 0) {
    fprintf(stderr, "%s\n", err);
  }

  libsql_free_rows(rows);
  libsql_free_stmt(stmt);

  int changes = libsql_changes(c);
  long long insert_row_id = libsql_last_insert_rowid(c);

  return {.type = SQLiteOk,
          .affectedRows = changes,
          .insertId = static_cast<double>(insert_row_id)};
}

/// Executes returning data in raw arrays, a small performance optimization
/// for certain use cases
BridgeResult
opsqlite_libsql_execute_raw(std::string const &name, std::string const &query,
                            const std::vector<JSVariant> *params,
                            std::vector<std::vector<JSVariant>> *results) {

  check_db_open(name);

  libsql_connection_t c = db_map[name].c;
  libsql_rows_t rows;
  libsql_row_t row;
  libsql_stmt_t stmt;
  int status = 0;
  const char *err = NULL;

  status = libsql_prepare(c, query.c_str(), &stmt, &err);

  if (status != 0) {
    return {.type = SQLiteError, .message = err};
  }

  if (params != nullptr && params->size() > 0) {
    opsqlite_libsql_bind_statement(stmt, params);
  }

  status = libsql_query_stmt(stmt, &rows, &err);

  if (status != 0) {
    return {.type = SQLiteError, .message = err};
  }

  int num_cols = libsql_column_count(rows);
  while ((status = libsql_next_row(rows, &row, &err)) == 0) {

    if (!err && !row) {
      break;
    }

    std::vector<JSVariant> row_vector;

    for (int col = 0; col < num_cols; col++) {
      int type;

      libsql_column_type(rows, row, col, &type, &err);

      switch (type) {
      case LIBSQL_INT:
        long long int_value;
        status = libsql_get_int(row, col, &int_value, &err);
        row_vector.push_back(JSVariant(int_value));
        break;

      case LIBSQL_FLOAT:
        double float_value;
        status = libsql_get_float(row, col, &float_value, &err);
        row_vector.push_back(JSVariant(float_value));
        break;

      case LIBSQL_TEXT:
        const char *text_value;
        status = libsql_get_string(row, col, &text_value, &err);
        row_vector.push_back(JSVariant(text_value));
        break;

      case LIBSQL_BLOB: {
        blob value_blob;
        libsql_get_blob(row, col, &value_blob, &err);
        uint8_t *data = new uint8_t[value_blob.len];
        // You cannot share raw memory between native and JS
        // always copy the data
        memcpy(data, value_blob.ptr, value_blob.len);
        libsql_free_blob(value_blob);
        row_vector.push_back(JSVariant(
            ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                        .size = static_cast<size_t>(value_blob.len)}));
        break;
      }

      case LIBSQL_NULL:
        // intentional fall-through
      default:
        row_vector.push_back(JSVariant(nullptr));
        break;
      }

      if (status != 0) {
        fprintf(stderr, "%s\n", err);
        throw std::runtime_error("libsql error");
      }
    }

    if (results != nullptr) {
      results->push_back(row_vector);
    }

    err = NULL;
  }

  if (status != 0) {
    fprintf(stderr, "%s\n", err);
  }

  libsql_free_rows(rows);
  libsql_free_stmt(stmt);

  int changes = libsql_changes(c);
  long long insert_row_id = libsql_last_insert_rowid(c);

  return {.type = SQLiteOk,
          .affectedRows = changes,
          .insertId = static_cast<double>(insert_row_id)};
}

BatchResult
opsqlite_libsql_execute_batch(std::string const &name,
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
    opsqlite_libsql_execute(name, "BEGIN EXCLUSIVE TRANSACTION", nullptr,
                            nullptr, nullptr);
    for (int i = 0; i < commandCount; i++) {
      auto command = commands->at(i);
      // We do not provide a datastructure to receive query data because we
      // don't need/want to handle this results in a batch execution
      auto result = opsqlite_libsql_execute(
          name, command.sql, command.params.get(), nullptr, nullptr);
      if (result.type == SQLiteError) {
        opsqlite_libsql_execute(name, "ROLLBACK", nullptr, nullptr, nullptr);
        return BatchResult{
            .type = SQLiteError,
            .message = result.message,
        };
      } else {
        affectedRows += result.affectedRows;
      }
    }
    opsqlite_libsql_execute(name, "COMMIT", nullptr, nullptr, nullptr);
    return BatchResult{
        .type = SQLiteOk,
        .affectedRows = affectedRows,
        .commands = static_cast<int>(commandCount),
    };
  } catch (std::exception &exc) {
    opsqlite_libsql_execute(name, "ROLLBACK", nullptr, nullptr, nullptr);
    return BatchResult{
        .type = SQLiteError,
        .message = exc.what(),
    };
  }
}

} // namespace opsqlite
