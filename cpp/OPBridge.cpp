// This file contains pure sqlite operations without JSI interaction
// Allows a clear defined boundary between the JSI and the SQLite operations
// so that threading operations are safe and contained within OPDatabase

#include "OPBridge.hpp"
#include "OPDatabase.hpp"
#include "OPDumbHostObject.hpp"
#include "OPSmartHostObject.hpp"
#include "OPLogs.h"
#include "OPUtils.hpp"
#include <filesystem>
#include <iostream>
#include <optional>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <variant>

#ifdef TOKENIZERS_HEADER_PATH
#include TOKENIZERS_HEADER_PATH
#else
#define TOKENIZER_LIST
#endif

namespace opsqlite {

/// The codes are on the error object as `code`/`extendedCode` once it reaches
/// JS, but they are also spelled out in the message so anything that only logs
/// the message still shows them. SQLite's own description stays in the middle
/// of the string, where a `toContain` style check still finds it.
static SQLiteError build_error(std::string const &context,
                               std::string const &message, int code,
                               int extended_code) {
  return {"[op-sqlite] " + context + ": " + message + " (code " +
              std::to_string(code) + ", extended code " +
              std::to_string(extended_code) + ")",
          code, extended_code};
}

/// Snapshots the connection's error state at the point of failure.
///
/// The returned error is built, not thrown, on purpose: sqlite3_errmsg hands
/// back a buffer owned by the connection which any later call on it -- both
/// sqlite3_reset and sqlite3_finalize included -- is free to overwrite or
/// free, and those same calls also move the result codes. Callers capture
/// first, clean up, and only then throw.
///
/// `status` is what the failing call returned; it is only consulted if the
/// connection somehow reports success.
///
/// The primary code is derived from the extended one rather than read through
/// sqlite3_errcode(), which returns the extended code on connections where
/// extended result codes have been switched on. SQLite guarantees the primary
/// code lives in the low 8 bits of every extended code, so `code` stays
/// primary either way.
static SQLiteError capture_error(sqlite3 *db, std::string const &context,
                                 int status) {
  int extended_code = sqlite3_extended_errcode(db);

  if (extended_code == SQLITE_OK) {
    extended_code = status;
  }

  int code = extended_code & 0xff;

  const char *message = sqlite3_errmsg(db);

  return build_error(context, message != nullptr ? message : "unknown error",
                     code, extended_code);
}

inline void opsqlite_bind_statement(sqlite3_stmt *statement,
                                    const std::vector<JSVariant> *values,
                                    bool should_clear_bindings) {
  if (should_clear_bindings) {
    sqlite3_clear_bindings(statement);
  }

  size_t size = values->size();

  for (int ii = 0; ii < size; ii++) {
    int stmt_index = ii + 1;
    JSVariant value = values->at(ii);

    std::visit(
        [&](auto &&v) {
          using T = std::decay_t<decltype(v)>;

          if constexpr (std::is_same_v<T, bool>) {
            sqlite3_bind_int(statement, stmt_index, static_cast<int>(v));
          } else if constexpr (std::is_same_v<T, int>) {
            sqlite3_bind_int(statement, stmt_index, v);
          } else if constexpr (std::is_same_v<T, long long>) {
            sqlite3_bind_double(statement, stmt_index, static_cast<double>(v));
          } else if constexpr (std::is_same_v<T, double>) {
            sqlite3_bind_double(statement, stmt_index, v);
          } else if constexpr (std::is_same_v<T, std::string>) {
            sqlite3_bind_text(statement, stmt_index, v.c_str(),
                              static_cast<int>(v.length()), SQLITE_TRANSIENT);
          } else if constexpr (std::is_same_v<T, ArrayBuffer>) {
            sqlite3_bind_blob(statement, stmt_index, v.data.get(),
                              static_cast<int>(v.size), SQLITE_TRANSIENT);
          } else {
            sqlite3_bind_null(statement, stmt_index);
          }
        },
        value);
  }
}

/// Returns the completely formed db path, but it also creates any sub-folders
/// along the way
std::string opsqlite_get_db_path(std::string const &db_name,
                                 std::string const &location) {

  if (location == ":memory:") {
    return location;
  }

  // Will return false if the directory already exists, no need to check
  std::filesystem::create_directories(location);

  if (!location.empty() && location.back() != '/') {
    return location + "/" + db_name;
  }

  return location + db_name;
}

#ifdef OP_SQLITE_USE_SQLCIPHER
sqlite3 *opsqlite_open(std::string const &name, std::string const &path,
                       bool readOnly, bool failOnCreate,
                       std::string const &encryption_key) {
#else
sqlite3 *opsqlite_open(std::string const &name, std::string const &path,
                       bool readOnly, bool failOnCreate) {
#endif
  std::string final_path = opsqlite_get_db_path(name, path);
  // Written to only on failure by both sqlite3_load_extension below and the
  // tokenizer init calls TOKENIZER_LIST expands into, so it starts out null.
  // Unused when neither of those is configured into the build.
  [[maybe_unused]] char *errMsg = nullptr;
  sqlite3 *db;

  int flags = SQLITE_OPEN_FULLMUTEX;
  if (readOnly) {
    flags |= SQLITE_OPEN_READONLY;
  } else {
    flags |= SQLITE_OPEN_READWRITE;
    if (!failOnCreate) {
      flags |= SQLITE_OPEN_CREATE;
    }
  }

  int status = sqlite3_open_v2(final_path.c_str(), &db, flags, nullptr);

  if (status != SQLITE_OK) {
    auto error = capture_error(db, "could not open database", status);
    sqlite3_close_v2(db);
    throw error;
  }

#ifdef OP_SQLITE_USE_SQLCIPHER
  if (!encryption_key.empty()) {
    // Use the SQLCipher C API directly instead of `PRAGMA key = '...'`.
    // Primary reason: removes the SQL-injection shape — if the key string
    // is ever attacker-influenced, a payload like `'; ATTACH ...; --`
    // would otherwise execute arbitrary SQL on the freshly-opened
    // connection. Secondary benefits: the key is passed as a binary
    // buffer with explicit length so embedded zero bytes in the key are
    // preserved, and it never enters trace/log surfaces — defense in
    // depth, not an exploit class on its own.
    int key_status = sqlite3_key_v2(db, "main", encryption_key.data(),
                                    static_cast<int>(encryption_key.size()));
    if (key_status != SQLITE_OK) {
      throw capture_error(db, "failed to set encryption key", key_status);
    }
  }
#endif

#ifndef OP_SQLITE_USE_PHONE_VERSION
  sqlite3_enable_load_extension(db, 1);
#endif

#ifdef OP_SQLITE_USE_SQLITE_VEC
  const char *vec_entry_point = "sqlite3_vec_init";

  int vec_status = sqlite3_load_extension(db, _sqlite_vec_path.c_str(),
                                          vec_entry_point, &errMsg);

  if (vec_status != SQLITE_OK) {
    std::string message = errMsg != nullptr ? errMsg : "unknown error";
    sqlite3_free(errMsg);

    throw build_error("could not load sqlite-vec", message, vec_status & 0xff,
                      vec_status);
  }
#endif

  TOKENIZER_LIST

  return db;
}

void create_dirs_if_needed(const std::string &path) {
  if (path == ":memory:") {
    return;
  }

  // Extract directory part from path (exclude filename)
  std::filesystem::path fs_path(path);
  auto dir = fs_path.parent_path();
  if (!dir.empty()) {
    std::filesystem::create_directories(dir);
  }
}

void opsqlite_close(sqlite3 *db) {
  sqlite3_close_v2(db);
}

void opsqlite_attach(sqlite3 *db, std::string const &doc_path,
                     std::string const &secondary_db_name,
                     std::string const &alias) {
  // ATTACH/DETACH's filename and schema-name slots both accept parameters
  // (verified on SQLite 3.50.6, behavior present across the 3.x series).
  // Binding sidesteps every escaping/identifier-quoting concern.
  auto secondary_db_path = opsqlite_get_db_path(secondary_db_name, doc_path);
  std::vector<JSVariant> params = {secondary_db_path, alias};
  opsqlite_execute(db, "ATTACH DATABASE ? AS ?", &params);
}

void opsqlite_detach(sqlite3 *db, std::string const &alias) {
  std::vector<JSVariant> params = {alias};
  opsqlite_execute(db, "DETACH DATABASE ?", &params);
}

void opsqlite_remove(sqlite3 *db, std::string const &name,
                     std::string const &doc_path) {
  opsqlite_close(db);

  std::string db_path = opsqlite_get_db_path(name, doc_path);

  if (!file_exists(db_path)) {
    throw std::runtime_error("op-sqlite: db file not found:" + db_path);
  }

  remove(db_path.c_str());
}

void opsqlite_remove_v2(sqlite3 *db, std::string const &path) {
  opsqlite_close(db);

  if (!file_exists(path)) {
    throw std::runtime_error("[op-sqlite] db file not found: " + path);
  }

  remove(path.c_str());
}

BridgeResult opsqlite_execute_prepared_statement(
    sqlite3 *db, sqlite3_stmt *statement, std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> &metadatas) {

  std::optional<SQLiteError> error;

  bool isConsuming = true;

  int result = SQLITE_OK;

  int i, count, column_type;
  std::string column_name, column_declared_type;

  while (isConsuming) {
    result = sqlite3_step(statement);

    switch (result) {
    case SQLITE_ROW: {
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
          // Specify length too; in case string contains NULL in the
          // middle
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
              ArrayBuffer{.data = std::shared_ptr<uint8_t[]>{data},
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

      results->emplace_back(row);

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

          metadatas->emplace_back(metadata);
          i++;
        }
      }
      isConsuming = false;
      break;

    default:
      error = capture_error(db, "statement execution error", result);
      isConsuming = false;
    }
  }

  sqlite3_reset(statement);

  if (error.has_value()) {
    throw *error;
  }

  int changedRowCount = sqlite3_changes(db);
  long long latestInsertRowId = sqlite3_last_insert_rowid(db);

  return {.affectedRows = changedRowCount,
          .insertId = static_cast<double>(latestInsertRowId)};
}

sqlite3_stmt *opsqlite_prepare_statement(sqlite3 *db,
                                         std::string const &query) {
  sqlite3_stmt *statement;

  const char *queryStr = query.c_str();

  int statementStatus =
      sqlite3_prepare_v2(db, queryStr, -1, &statement, nullptr);

  // Any non-OK status means there is no statement to hand back. Only
  // SQLITE_ERROR used to be caught here, so a prepare that failed with e.g.
  // SQLITE_BUSY or SQLITE_NOTADB returned a null statement as if it had
  // succeeded.
  if (statementStatus != SQLITE_OK) {
    throw capture_error(db, "SQL prepare statement error", statementStatus);
  }

  return statement;
}

void opsqlite_finalize_statement(sqlite3_stmt *statement) {
  if (statement != nullptr) {
    sqlite3_finalize(statement);
  }
}

BridgeResult opsqlite_execute(sqlite3 *db, std::string const &query,
                              const std::vector<JSVariant> *params) {
  sqlite3_stmt *statement;
  const char *remainingStatement = nullptr;
  std::optional<SQLiteError> error;
  int status, current_column, column_count, column_type;
  std::string column_name, column_declared_type;
  std::vector<std::string> column_names;
  std::vector<std::vector<JSVariant>> rows;
  std::vector<JSVariant> row;
  int changedRowCount = 0;
  long long latestInsertRowId = 0;

  do {
    const char *query_str =
        remainingStatement == nullptr ? query.c_str() : remainingStatement;

    status =
        sqlite3_prepare_v2(db, query_str, -1, &statement, &remainingStatement);

    if (status != SQLITE_OK) {
      throw capture_error(db, "sqlite query error", status);
    }

    // The statement did not fail to parse but there is nothing to do, just
    // skip to the end
    if (statement == nullptr) {
      continue;
    }

    if (params != nullptr && !params->empty()) {
      opsqlite_bind_statement(statement, params, /* should_clear_bindings */ false);
    }

    // sqlite3_column_count is the correct signal: it's non-zero for any
    // statement that can return rows (SELECT, and write statements with
    // RETURNING), regardless of sqlite3_stmt_readonly.
    column_names.clear();
    column_count = sqlite3_column_count(statement);
    if (column_count > 0) {
      column_names.reserve(column_count);
      for (int i = 0; i < column_count; i++) {
        column_name = sqlite3_column_name(statement, i);
        column_names.emplace_back(column_name);
      }
    }

    bool is_consuming_rows = true;
    double double_value;
    const char *string_value;

    while (is_consuming_rows) {
      status = sqlite3_step(statement);

      switch (status) {
      case SQLITE_DONE:
        changedRowCount = sqlite3_changes(db);
        latestInsertRowId = sqlite3_last_insert_rowid(db);
        is_consuming_rows = false;
        break;

      case SQLITE_ROW:
        current_column = 0;
        row = std::vector<JSVariant>();
        row.reserve(column_count);

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
            int len = sqlite3_column_bytes(statement, current_column);
            // Specify length too; in case string contains NULL in
            // the middle
            row.emplace_back(std::string(string_value, len));
            break;
          }

          case SQLITE_BLOB: {
            int blob_size = sqlite3_column_bytes(statement, current_column);
            const void *blob = sqlite3_column_blob(statement, current_column);
            auto *data = new uint8_t[blob_size];
            memcpy(data, blob, blob_size);
            row.emplace_back(
                ArrayBuffer{.data = std::shared_ptr<uint8_t[]>{data},
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

        rows.emplace_back(std::move(row));
        break;

      default:
        // Captured before the finalize below, which resets the connection's
        // error state and invalidates the message buffer.
        error = capture_error(db, "statement execution error", status);
        is_consuming_rows = false;
      }
    }

    sqlite3_finalize(statement);

  } while (remainingStatement != nullptr &&
           strcmp(remainingStatement, "") != 0 && !error.has_value());

  if (error.has_value()) {
    throw *error;
  }

  return {.affectedRows = changedRowCount,
          .insertId = static_cast<double>(latestInsertRowId),
          .rows = std::move(rows),
          .column_names = std::move(column_names)};
}

BridgeResult opsqlite_execute_host_objects(
    sqlite3 *db, std::string const &query, const std::vector<JSVariant> *params,
    std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> &metadatas) {

  sqlite3_stmt *statement;
  const char *remainingStatement = nullptr;
  std::optional<SQLiteError> error;

  bool isConsuming = true;

  int result = SQLITE_OK;

  do {
    const char *queryStr =
        remainingStatement == nullptr ? query.c_str() : remainingStatement;

    int statementStatus =
        sqlite3_prepare_v2(db, queryStr, -1, &statement, &remainingStatement);

    if (statementStatus != SQLITE_OK) {
      throw capture_error(db, "SQL statement error on opsqlite_execute",
                          statementStatus);
    }

    // The statement did not fail to parse but there is nothing to do, just
    // skip to the end
    if (statement == nullptr) {
      continue;
    }

    if (params != nullptr && !params->empty()) {
      opsqlite_bind_statement(statement, params, /* should_clear_bindings */ false);
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
            // Specify length too; in case string contains NULL in
            // the middle
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
                ArrayBuffer{.data = std::shared_ptr<uint8_t[]>{data},
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

        results->emplace_back(row);
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
        // Captured before the finalize below, which resets the connection's
        // error state and invalidates the message buffer.
        error = capture_error(db, "statement execution error", result);
        isConsuming = false;
      }
    }

    sqlite3_finalize(statement);
  } while (remainingStatement != nullptr &&
           strcmp(remainingStatement, "") != 0 && !error.has_value());

  if (error.has_value()) {
    throw *error;
  }

  int changedRowCount = sqlite3_changes(db);
  long long latestInsertRowId = sqlite3_last_insert_rowid(db);

  return {.affectedRows = changedRowCount,
          .insertId = static_cast<double>(latestInsertRowId)};
}

/// Executes returning data in raw arrays
BridgeResult
opsqlite_execute_raw(sqlite3 *db, std::string const &query,
                     const std::vector<JSVariant> *params,
                     std::vector<std::vector<JSVariant>> *results) {
  sqlite3_stmt *statement;
  const char *remainingStatement = nullptr;
  std::optional<SQLiteError> error;

  bool isConsuming = true;

  int step = SQLITE_OK;
  std::vector<std::string> column_names;

  do {
    const char *queryStr =
        remainingStatement == nullptr ? query.c_str() : remainingStatement;

    int statementStatus =
        sqlite3_prepare_v2(db, queryStr, -1, &statement, &remainingStatement);

    if (statementStatus != SQLITE_OK) {
      throw capture_error(db, "SQL statement error", statementStatus);
    }

    // The statement did not fail to parse but there is nothing to do, just
    // skip to the end
    if (statement == nullptr) {
      continue;
    }

    if (params != nullptr && !params->empty()) {
      opsqlite_bind_statement(statement, params, /* should_clear_bindings */ false);
    }

    int i, column_type;
    std::string column_name, column_declared_type;

    int column_count = sqlite3_column_count(statement);

    column_names.clear();
    column_names.reserve(column_count);
    for (int column_index = 0; column_index < column_count; column_index++) {
      column_name = sqlite3_column_name(statement, column_index);
      column_names.emplace_back(column_name);
    }

    while (isConsuming) {
      step = sqlite3_step(statement);

      switch (step) {
      case SQLITE_ROW: {
        if (results == nullptr) {
          break;
        }

        std::vector<JSVariant> row;
        row.reserve(column_count);

        i = 0;

        while (i < column_count) {
          column_type = sqlite3_column_type(statement, i);

          switch (column_type) {
          case SQLITE_INTEGER:
          case SQLITE_FLOAT: {
            double column_value = sqlite3_column_double(statement, i);
            row.emplace_back(column_value);
            break;
          }

          case SQLITE_TEXT: {
            const char *column_value = reinterpret_cast<const char *>(
                sqlite3_column_text(statement, i));
            int byteLen = sqlite3_column_bytes(statement, i);
            // Specify length too; in case string contains NULL in
            // the middle
            row.emplace_back(std::string(column_value, byteLen));
            break;
          }

          case SQLITE_BLOB: {
            int blob_size = sqlite3_column_bytes(statement, i);
            const void *blob = sqlite3_column_blob(statement, i);
            auto *data = new uint8_t[blob_size];
            memcpy(data, blob, blob_size);
            row.emplace_back(
                ArrayBuffer{.data = std::shared_ptr<uint8_t[]>{data},
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

        results->emplace_back(row);

        break;
      }

      case SQLITE_DONE:
        isConsuming = false;
        break;

      default:
        // Captured before the finalize below, which resets the connection's
        // error state and invalidates the message buffer.
        error = capture_error(db, "statement execution error", step);
        isConsuming = false;
      }
    }

    sqlite3_finalize(statement);
  } while (remainingStatement != nullptr &&
           strcmp(remainingStatement, "") != 0 && !error.has_value());

  if (error.has_value()) {
    throw *error;
  }

  int changedRowCount = sqlite3_changes(db);
  long long latestInsertRowId = sqlite3_last_insert_rowid(db);

  return {.affectedRows = changedRowCount,
          .insertId = static_cast<double>(latestInsertRowId),
          .column_names = std::move(column_names)};
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
    throw std::runtime_error("Unknown SQLite operation on hook");
  }
}

void update_callback(void *opsqlite_db_ptr, int operation_type,
                     [[maybe_unused]] char const *database, char const *table,
                     sqlite3_int64 row_id) {
  auto opsqlite_db = reinterpret_cast<OPDatabase *>(opsqlite_db_ptr);
  opsqlite_db->on_update(std::string(table),
                         operation_to_string(operation_type), row_id);
}

void opsqlite_register_update_hook(sqlite3 *db, void *opsqlite_db_ptr) {
  sqlite3_update_hook(db, &update_callback, opsqlite_db_ptr);
}

void opsqlite_deregister_update_hook(sqlite3 *db) {
  sqlite3_update_hook(db, nullptr, nullptr);
}

int commit_callback(void *opsqlite_db_ptr) {
  auto opsqlite_db = reinterpret_cast<OPDatabase *>(opsqlite_db_ptr);
  opsqlite_db->on_commit();
  return 0;
}

void opsqlite_register_commit_hook(sqlite3 *db, void *opsqlite_db_ptr) {
  sqlite3_commit_hook(db, &commit_callback, opsqlite_db_ptr);
}

void opsqlite_deregister_commit_hook(sqlite3 *db) {
  sqlite3_commit_hook(db, nullptr, nullptr);
}

void rollback_callback(void *opsqlite_db_ptr) {
  auto opsqlite_db = reinterpret_cast<OPDatabase *>(opsqlite_db_ptr);
  opsqlite_db->on_rollback();
}

void opsqlite_register_rollback_hook(sqlite3 *db, void *opsqlite_db_ptr) {
  sqlite3_rollback_hook(db, &rollback_callback, opsqlite_db_ptr);
}

void opsqlite_deregister_rollback_hook(sqlite3 *db) {
  sqlite3_rollback_hook(db, nullptr, nullptr);
}

void opsqlite_load_extension(sqlite3 *db, std::string &path,
                             std::string &entry_point) {
#ifdef OP_SQLITE_USE_PHONE_VERSION
  throw std::runtime_error("[op-sqlite] Embedded version of SQLite does not "
                           "support loading extensions");
#else
  int status = 0;
  status = sqlite3_enable_load_extension(db, 1);

  if (status != SQLITE_OK) {
    throw capture_error(db, "could not enable extension loading", status);
  }

  const char *entry_point_cstr = nullptr;
  if (!entry_point.empty()) {
    entry_point_cstr = entry_point.c_str();
  }

  char *error_message = nullptr;

  status = sqlite3_load_extension(db, path.c_str(), entry_point_cstr,
                                  &error_message);
  if (status != SQLITE_OK) {
    // This message is allocated by sqlite3, it does not live on the connection
    // like sqlite3_errmsg's does, so it has to be copied out and freed here.
    std::string message =
        error_message != nullptr ? error_message : "unknown error";
    sqlite3_free(error_message);

    throw build_error("could not load extension", message, status & 0xff,
                      status);
  }
#endif
}

BatchResult
opsqlite_execute_batch(sqlite3 *db,
                       const std::vector<BatchArguments> *commands) {
  size_t commandCount = commands->size();
  if (commandCount <= 0) {
    throw std::runtime_error("No SQL commands provided");
  }

  int affectedRows = 0;
  // opsqlite_execute(db, "BEGIN EXCLUSIVE TRANSACTION", nullptr);
  for (int i = 0; i < commandCount; i++) {
    const auto &command = commands->at(i);
    // We do not provide a datastructure to receive query data because we
    // don't need/want to handle this results in a batch execution
    // There is also no need to commit/catch this transaction, this is done
    // in the JS code
    auto result = opsqlite_execute(db, command.sql, &command.params);
    affectedRows += result.affectedRows;
  }

  return BatchResult{
      .affectedRows = affectedRows,
      .commands = static_cast<int>(commandCount),
  };
}

} // namespace opsqlite
