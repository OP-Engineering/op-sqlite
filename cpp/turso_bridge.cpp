#include "bridge.h"
#include "DBHostObject.h"
#include "DumbHostObject.h"
#include "SmartHostObject.h"
#include "utils.hpp"

#ifdef __APPLE__
extern "C" {
#include <turso_sdk_kit/turso.h>
#include <turso_sdk_kit/turso_sync.h>
}
#else
extern "C" {
#include "turso.h"
#include "turso_sync.h"
}
#endif

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace opsqlite {

namespace {

struct TursoDbHandle {
  const turso_database_t *database = nullptr;
  const turso_sync_database_t *sync_database = nullptr;
  const turso_connection_t *connection = nullptr;
  bool is_sync = false;
  std::string path;
};

struct TursoStmtHandle {
  turso_statement_t *statement = nullptr;
  TursoDbHandle *db = nullptr;
};

inline TursoDbHandle *to_turso_db(sqlite3 *db) {
  return reinterpret_cast<TursoDbHandle *>(db);
}

inline TursoStmtHandle *to_turso_stmt(sqlite3_stmt *statement) {
  return reinterpret_cast<TursoStmtHandle *>(statement);
}

inline const turso_connection_t *require_turso_connection(
    TursoDbHandle *handle, const std::string &context) {
  if (handle == nullptr || handle->connection == nullptr) {
    throw std::runtime_error("[op-sqlite][turso] " + context +
                             ": invalid database connection");
  }

  return handle->connection;
}

void throw_if_turso_error(turso_status_code_t code, const char *error,
                          const std::string &context) {
  if (code == TURSO_OK || code == TURSO_ROW || code == TURSO_DONE) {
    return;
  }

  throw std::runtime_error(
      "[op-sqlite][turso] " + context +
      (error != nullptr ? ": " + std::string(error) : ""));
}

std::vector<char> read_binary_file(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return {};
  }

  file.seekg(0, std::ios::end);
  const auto size = file.tellg();
  if (size <= 0) {
    return {};
  }

  std::vector<char> content(static_cast<size_t>(size));
  file.seekg(0, std::ios::beg);
  file.read(content.data(), size);
  return content;
}

void write_binary_file_atomic(const std::string &path,
                              const char *data,
                              size_t size) {
  std::filesystem::path target(path);
  std::filesystem::create_directories(target.parent_path());

  const std::filesystem::path temp = target.string() + ".opsqlite.tmp";

  {
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
      throw std::runtime_error("[op-sqlite][turso] failed to open temp file for write: " +
                               temp.string());
    }

    if (size > 0) {
      file.write(data, static_cast<std::streamsize>(size));
    }

    if (!file.good()) {
      throw std::runtime_error("[op-sqlite][turso] failed writing temp file: " +
                               temp.string());
    }
  }

  std::error_code ec;
  std::filesystem::rename(temp, target, ec);
  if (ec) {
    std::filesystem::remove(target, ec);
    ec.clear();
    std::filesystem::rename(temp, target, ec);
  }

  if (ec) {
    throw std::runtime_error("[op-sqlite][turso] failed to atomically replace file: " +
                             path);
  }
}

void setup_turso_temp_dir(const std::string &db_path) {
  if (db_path == ":memory:") {
    return;
  }

  std::filesystem::path db_dir(db_path);
  if (db_dir.has_filename()) {
    db_dir = db_dir.parent_path();
  }

  if (db_dir.empty()) {
    return;
  }

  std::error_code ec;
  std::filesystem::create_directories(db_dir, ec);
  if (ec) {
    return;
  }

  auto temp_dir = (db_dir / ".turso-tmp").string();
  std::filesystem::create_directories(temp_dir, ec);
  if (ec) {
    return;
  }

  // Keep temp files in app-writable storage instead of restricted emulator temp dirs.
  setenv("TMPDIR", temp_dir.c_str(), 1);
  setenv("SQLITE_TMPDIR", temp_dir.c_str(), 1);
  setenv("TMP", temp_dir.c_str(), 1);
  setenv("TEMP", temp_dir.c_str(), 1);
}

void process_sync_io_item(const turso_sync_io_item_t *item) {
  const auto kind = turso_sync_database_io_request_kind(item);

  if (kind == TURSO_SYNC_IO_HTTP) {
    std::string message =
        "[op-sqlite][turso] sync HTTP IO request is not supported by native op-sqlite Turso bridge yet";
    turso_slice_ref_t error = {.ptr = message.c_str(), .len = message.size()};
    turso_sync_database_io_poison(item, &error);
    return;
  }

  if (kind == TURSO_SYNC_IO_FULL_READ) {
    turso_sync_io_full_read_request_t request = {};
    if (turso_sync_database_io_request_full_read(item, &request) != TURSO_OK) {
      std::string message = "failed to decode FULL_READ request";
      turso_slice_ref_t error = {.ptr = message.c_str(), .len = message.size()};
      turso_sync_database_io_poison(item, &error);
      return;
    }

    std::string path(static_cast<const char *>(request.path.ptr), request.path.len);
    auto buffer = read_binary_file(path);
    if (!buffer.empty()) {
      turso_slice_ref_t slice = {.ptr = buffer.data(), .len = buffer.size()};
      if (turso_sync_database_io_push_buffer(item, &slice) != TURSO_OK) {
        std::string message = "failed to push FULL_READ data";
        turso_slice_ref_t error = {.ptr = message.c_str(), .len = message.size()};
        turso_sync_database_io_poison(item, &error);
        return;
      }
    }

    turso_sync_database_io_done(item);
    return;
  }

  if (kind == TURSO_SYNC_IO_FULL_WRITE) {
    turso_sync_io_full_write_request_t request = {};
    if (turso_sync_database_io_request_full_write(item, &request) != TURSO_OK) {
      std::string message = "failed to decode FULL_WRITE request";
      turso_slice_ref_t error = {.ptr = message.c_str(), .len = message.size()};
      turso_sync_database_io_poison(item, &error);
      return;
    }

    std::string path(static_cast<const char *>(request.path.ptr), request.path.len);

    try {
      write_binary_file_atomic(path,
                               static_cast<const char *>(request.content.ptr),
                               request.content.len);
      turso_sync_database_io_done(item);
    } catch (const std::exception &e) {
      const std::string message = e.what();
      turso_slice_ref_t error = {.ptr = message.c_str(), .len = message.size()};
      turso_sync_database_io_poison(item, &error);
    }

    return;
  }

  turso_sync_database_io_done(item);
}

void drain_sync_io(const turso_sync_database_t *db) {
  const char *error = nullptr;

  while (true) {
    const turso_sync_io_item_t *item = nullptr;
    const auto status = turso_sync_database_io_take_item(db, &item, &error);
    throw_if_turso_error(status, error, "take sync io item");

    if (item == nullptr) {
      break;
    }

    process_sync_io_item(item);
    turso_sync_database_io_item_deinit(item);
  }

  throw_if_turso_error(turso_sync_database_io_step_callbacks(db, &error), error,
                       "step sync io callbacks");
}

void run_sync_operation(const turso_sync_database_t *db,
                        const turso_sync_operation_t *operation) {
  const char *error = nullptr;

  while (true) {
    const auto status = turso_sync_operation_resume(operation, &error);

    if (status == TURSO_IO) {
      drain_sync_io(db);
      continue;
    }

    if (status == TURSO_DONE) {
      break;
    }

    throw_if_turso_error(status, error, "resume sync operation");
  }
}

void bind_value(turso_statement_t *statement, size_t position,
                const JSVariant &value) {
  turso_status_code_t code = TURSO_OK;

  if (std::holds_alternative<bool>(value)) {
    code = turso_statement_bind_positional_int(
        statement, position, std::get<bool>(value) ? 1 : 0);
  } else if (std::holds_alternative<int>(value)) {
    code = turso_statement_bind_positional_int(statement, position,
                                               std::get<int>(value));
  } else if (std::holds_alternative<long>(value)) {
    code = turso_statement_bind_positional_int(statement, position,
                                               std::get<long>(value));
  } else if (std::holds_alternative<long long>(value)) {
    code = turso_statement_bind_positional_int(statement, position,
                                               std::get<long long>(value));
  } else if (std::holds_alternative<double>(value)) {
    code = turso_statement_bind_positional_double(statement, position,
                                                  std::get<double>(value));
  } else if (std::holds_alternative<std::string>(value)) {
    const auto &str = std::get<std::string>(value);
    code = turso_statement_bind_positional_text(statement, position,
                                                str.c_str(), str.size());
  } else if (std::holds_alternative<ArrayBuffer>(value)) {
    const auto &blob = std::get<ArrayBuffer>(value);
    code = turso_statement_bind_positional_blob(
        statement, position, reinterpret_cast<const char *>(blob.data.get()),
        blob.size);
  } else {
    code = turso_statement_bind_positional_null(statement, position);
  }

  throw_if_turso_error(code, nullptr, "bind parameter");
}

void run_step_loop(turso_statement_t *statement,
                   const std::function<void()> &on_row) {
  const char *error = nullptr;

  while (true) {
    auto code = turso_statement_step(statement, &error);

    if (code == TURSO_IO) {
      throw_if_turso_error(turso_statement_run_io(statement, &error), error,
                           "run io");
      continue;
    }

    if (code == TURSO_ROW) {
      on_row();
      continue;
    }

    if (code == TURSO_DONE) {
      break;
    }

    throw_if_turso_error(code, error, "step statement");
  }
}

void reset_statement(turso_statement_t *statement) {
  const char *error = nullptr;
  auto code = turso_statement_reset(statement, &error);

  if (code == TURSO_IO) {
    throw_if_turso_error(turso_statement_run_io(statement, &error), error,
                         "run io while reset");
    code = turso_statement_reset(statement, &error);
  }

  throw_if_turso_error(code, error, "reset statement");
}

} // namespace

void opsqlite_bind_statement(sqlite3_stmt *statement,
                             const std::vector<JSVariant> *values) {
  auto *stmt = to_turso_stmt(statement);

  for (size_t i = 0; i < values->size(); i++) {
    bind_value(stmt->statement, i + 1, values->at(i));
  }
}

std::string opsqlite_get_db_path(std::string const &db_name,
                                 std::string const &location) {

  if (location == ":memory:") {
    return location;
  }

  std::filesystem::create_directories(location);

  if (!location.empty() && location.back() != '/') {
    return location + "/" + db_name;
  }

  return location + db_name;
}

sqlite3 *opsqlite_open(std::string const &name, std::string const &path,
                       [[maybe_unused]] std::string const &crsqlite_path,
                       [[maybe_unused]] std::string const &sqlite_vec_path) {
  auto *handle = new TursoDbHandle();
  handle->path = opsqlite_get_db_path(name, path);
  setup_turso_temp_dir(handle->path);

  turso_database_config_t db_config = {
      .async_io = 0,
    .path = handle->path.c_str(),
      .experimental_features = nullptr,
      .vfs = nullptr,
      .encryption_cipher = nullptr,
      .encryption_hexkey = nullptr,
  };

  const char *error = nullptr;
  const turso_database_t *database = nullptr;

  try {
    throw_if_turso_error(
        turso_database_new(&db_config, &database, &error), error,
        "create database at " + handle->path);
    throw_if_turso_error(turso_database_open(database, &error), error,
                         "open database at " + handle->path);

    turso_connection_t *connection = nullptr;
    throw_if_turso_error(
        turso_database_connect(database, &connection, &error), error,
        "connect database at " + handle->path);

    handle->database = database;
    handle->connection = connection;
  } catch (...) {
    if (handle->connection != nullptr) {
      turso_connection_deinit(handle->connection);
      handle->connection = nullptr;
    }

    if (database != nullptr) {
      turso_database_deinit(database);
    }

    delete handle;
    throw;
  }

  return reinterpret_cast<sqlite3 *>(handle);
}

sqlite3 *opsqlite_open_sync(std::string const &name, std::string const &path,
                            std::string const &url,
                            [[maybe_unused]] std::string const &auth_token,
                            std::string const &remote_encryption_key) {
  auto *handle = new TursoDbHandle();
  handle->path = opsqlite_get_db_path(name, path);
  setup_turso_temp_dir(handle->path);

  turso_database_config_t db_config = {
      // Keep op-sqlite API synchronous by letting the Turso runtime perform IO.
      .async_io = 0,
      .path = handle->path.c_str(),
      .experimental_features = nullptr,
      .vfs = nullptr,
      .encryption_cipher = nullptr,
      .encryption_hexkey = nullptr,
  };

  turso_sync_database_config_t sync_config = {
      .path = handle->path.c_str(),
      .remote_url = url.c_str(),
      .client_name = "op-sqlite",
      .long_poll_timeout_ms = 0,
      .bootstrap_if_empty = true,
      .reserved_bytes = 0,
      .partial_bootstrap_strategy_prefix = 0,
      .partial_bootstrap_strategy_query = nullptr,
      .partial_bootstrap_segment_size = 0,
      .partial_bootstrap_prefetch = false,
      .remote_encryption_key =
          remote_encryption_key.empty() ? nullptr : remote_encryption_key.c_str(),
      .remote_encryption_cipher = nullptr,
  };

  const char *error = nullptr;
  const turso_sync_database_t *sync_database = nullptr;

  try {
    throw_if_turso_error(
        turso_sync_database_new(&db_config, &sync_config, &sync_database, &error),
        error, "create sync database at " + handle->path);

    const turso_sync_operation_t *open_operation = nullptr;
    throw_if_turso_error(
        turso_sync_database_create(sync_database, &open_operation, &error), error,
        "open/create sync database at " + handle->path);
    run_sync_operation(sync_database, open_operation);
    turso_sync_operation_deinit(open_operation);

    const turso_sync_operation_t *connect_operation = nullptr;
    throw_if_turso_error(
        turso_sync_database_connect(sync_database, &connect_operation, &error),
        error, "connect sync database at " + handle->path);
    run_sync_operation(sync_database, connect_operation);

    if (turso_sync_operation_result_kind(connect_operation) !=
        TURSO_ASYNC_RESULT_CONNECTION) {
      turso_sync_operation_deinit(connect_operation);
      throw std::runtime_error("[op-sqlite][turso] sync connect did not return a connection");
    }

    const turso_connection_t *connection = nullptr;
    throw_if_turso_error(turso_sync_operation_result_extract_connection(
                             connect_operation, &connection),
                         nullptr, "extract sync connection");
    turso_sync_operation_deinit(connect_operation);

    handle->database = nullptr;
    handle->sync_database = sync_database;
    handle->connection = connection;
    handle->is_sync = true;
  } catch (...) {
    if (handle->connection != nullptr) {
      turso_connection_deinit(handle->connection);
      handle->connection = nullptr;
    }

    if (sync_database != nullptr) {
      turso_sync_database_deinit(sync_database);
    }

    delete handle;
    throw;
  }

  return reinterpret_cast<sqlite3 *>(handle);
}

sqlite3 *opsqlite_open_remote(std::string const &url,
                              std::string const &auth_token,
                              std::string const &base_path) {
  std::string remote_name =
      "turso_remote_" + std::to_string(std::hash<std::string>{}(url)) +
      ".sqlite";
  return opsqlite_open_sync(remote_name, base_path, url, auth_token, "");
}

void opsqlite_close(sqlite3 *db) {
  auto *handle = to_turso_db(db);
  if (handle == nullptr) {
    return;
  }

  if (handle->connection != nullptr) {
    turso_connection_deinit(handle->connection);
    handle->connection = nullptr;
  }

  if (handle->database != nullptr) {
    turso_database_deinit(handle->database);
    handle->database = nullptr;
  }

  if (handle->sync_database != nullptr) {
    turso_sync_database_deinit(handle->sync_database);
    handle->sync_database = nullptr;
  }

  delete handle;
}

void opsqlite_sync(sqlite3 *db) {
  auto *handle = to_turso_db(db);
  if (handle == nullptr || handle->sync_database == nullptr) {
    throw std::runtime_error("[op-sqlite][turso] sync is only available for sync/remote databases");
  }

  const char *error = nullptr;

  const turso_sync_operation_t *push_operation = nullptr;
  throw_if_turso_error(
      turso_sync_database_push_changes(handle->sync_database, &push_operation,
                                       &error),
      error, "push sync changes");
  run_sync_operation(handle->sync_database, push_operation);
  turso_sync_operation_deinit(push_operation);

  const turso_sync_operation_t *wait_operation = nullptr;
  throw_if_turso_error(
      turso_sync_database_wait_changes(handle->sync_database, &wait_operation,
                                       &error),
      error, "wait sync changes");
  run_sync_operation(handle->sync_database, wait_operation);

  const turso_sync_changes_t *changes = nullptr;
  throw_if_turso_error(
      turso_sync_operation_result_extract_changes(wait_operation, &changes),
      nullptr, "extract sync changes");
  turso_sync_operation_deinit(wait_operation);

  if (changes != nullptr) {
    const turso_sync_operation_t *apply_operation = nullptr;
    throw_if_turso_error(
        turso_sync_database_apply_changes(handle->sync_database, changes,
                                          &apply_operation, &error),
        error, "apply sync changes");
    run_sync_operation(handle->sync_database, apply_operation);
    turso_sync_operation_deinit(apply_operation);
  }
}

void opsqlite_remove(sqlite3 *db, std::string const &name,
                     std::string const &doc_path) {
  std::string db_path = opsqlite_get_db_path(name, doc_path);
  opsqlite_close(db);

  if (!file_exists(db_path)) {
    throw std::runtime_error("[op-sqlite] db file not found:" + db_path);
  }

  remove(db_path.c_str());
}

void opsqlite_attach(sqlite3 *db, std::string const &doc_path,
                     std::string const &secondary_db_name,
                     std::string const &alias) {
  auto secondary_db_path = opsqlite_get_db_path(secondary_db_name, doc_path);
  auto statement = "ATTACH DATABASE '" + secondary_db_path + "' AS " + alias;
  opsqlite_execute(db, statement, nullptr);
}

void opsqlite_detach(sqlite3 *db, std::string const &alias) {
  opsqlite_execute(db, "DETACH DATABASE " + alias, nullptr);
}

sqlite3_stmt *opsqlite_prepare_statement(sqlite3 *db,
                                         std::string const &query) {
  auto *handle = to_turso_db(db);
  turso_statement_t *statement = nullptr;
  const char *error = nullptr;

  throw_if_turso_error(turso_connection_prepare_single(
               require_turso_connection(handle, "prepare statement"),
               query.c_str(), &statement, &error),
                     error, "prepare statement");

  auto *stmt_handle = new TursoStmtHandle();
  stmt_handle->statement = statement;
  stmt_handle->db = handle;

  return reinterpret_cast<sqlite3_stmt *>(stmt_handle);
}

void opsqlite_finalize_statement(sqlite3_stmt *statement) {
  auto *stmt = to_turso_stmt(statement);
  if (stmt == nullptr) {
    return;
  }

  if (stmt->statement != nullptr) {
    turso_statement_deinit(stmt->statement);
    stmt->statement = nullptr;
  }

  delete stmt;
}

BridgeResult opsqlite_execute_prepared_statement(
    sqlite3 *db, sqlite3_stmt *statement, std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> &metadatas) {
  auto *db_handle = to_turso_db(db);
  auto *stmt = to_turso_stmt(statement);
  int changes = 0;

  run_step_loop(stmt->statement, [&]() {
    if (results == nullptr) {
      return;
    }

    int col_count = static_cast<int>(turso_statement_column_count(stmt->statement));
    DumbHostObject row = DumbHostObject(metadatas);

    for (int i = 0; i < col_count; i++) {
      auto kind = turso_statement_row_value_kind(stmt->statement, i);

      switch (kind) {
      case TURSO_TYPE_INTEGER:
        row.values.emplace_back(
            static_cast<double>(turso_statement_row_value_int(stmt->statement, i)));
        break;
      case TURSO_TYPE_REAL:
        row.values.emplace_back(turso_statement_row_value_double(stmt->statement, i));
        break;
      case TURSO_TYPE_TEXT: {
        auto size = turso_statement_row_value_bytes_count(stmt->statement, i);
        auto ptr = turso_statement_row_value_bytes_ptr(stmt->statement, i);
        row.values.emplace_back(std::string(ptr, static_cast<size_t>(size)));
        break;
      }
      case TURSO_TYPE_BLOB: {
        auto size = turso_statement_row_value_bytes_count(stmt->statement, i);
        auto ptr = turso_statement_row_value_bytes_ptr(stmt->statement, i);
        auto *data = new uint8_t[static_cast<size_t>(size)];
        memcpy(data, ptr, static_cast<size_t>(size));
        row.values.emplace_back(ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                                            .size = static_cast<size_t>(size)});
        break;
      }
      case TURSO_TYPE_NULL:
      case TURSO_TYPE_UNKNOWN:
      default:
        row.values.emplace_back(nullptr);
      }
    }

    results->emplace_back(row);
  });

  if (metadatas != nullptr && metadatas->empty()) {
    int col_count = static_cast<int>(turso_statement_column_count(stmt->statement));

    for (int i = 0; i < col_count; i++) {
      auto metadata = SmartHostObject();

      const char *name = turso_statement_column_name(stmt->statement, i);
      const char *type = turso_statement_column_decltype(stmt->statement, i);

      metadata.fields.emplace_back("name", name == nullptr ? "" : name);
      metadata.fields.emplace_back("index", i);
      metadata.fields.emplace_back("type", type == nullptr ? "UNKNOWN" : type);

      if (name != nullptr) {
        turso_str_deinit(name);
      }
      if (type != nullptr) {
        turso_str_deinit(type);
      }

      metadatas->emplace_back(metadata);
    }
  }

  changes = static_cast<int>(turso_statement_n_change(stmt->statement));

  reset_statement(stmt->statement);

  return {.affectedRows = changes,
        .insertId = static_cast<double>(turso_connection_last_insert_rowid(
          require_turso_connection(db_handle, "last_insert_rowid")))};
}

BridgeResult opsqlite_execute(sqlite3 *db, std::string const &query,
                              const std::vector<JSVariant> *params) {
  auto *db_handle = to_turso_db(db);
  std::vector<std::vector<JSVariant>> rows;
  std::vector<std::string> column_names;
  size_t offset = 0;
  int changes = 0;

  while (offset < query.size()) {
    const char *error = nullptr;
    turso_statement_t *statement = nullptr;
    size_t tail = 0;

    auto code = turso_connection_prepare_first(
      require_turso_connection(db_handle, "prepare statement in batch execute"),
      query.c_str() + offset, &statement, &tail, &error);
    throw_if_turso_error(code, error, "prepare statement in batch execute");

    if (tail == 0) {
      break;
    }

    offset += tail;

    if (statement == nullptr) {
      continue;
    }

    if (params != nullptr && !params->empty()) {
      for (size_t i = 0; i < params->size(); i++) {
        bind_value(statement, i + 1, params->at(i));
      }
    }

    int col_count = static_cast<int>(turso_statement_column_count(statement));
    if (column_names.empty() && col_count > 0) {
      column_names.reserve(col_count);
      for (int i = 0; i < col_count; i++) {
        const char *name = turso_statement_column_name(statement, i);
        column_names.emplace_back(name == nullptr ? "" : name);
        if (name != nullptr) {
          turso_str_deinit(name);
        }
      }
    }

    run_step_loop(statement, [&]() {
      std::vector<JSVariant> row;
      row.reserve(col_count);

      for (int i = 0; i < col_count; i++) {
        auto kind = turso_statement_row_value_kind(statement, i);
        switch (kind) {
        case TURSO_TYPE_INTEGER:
          row.emplace_back(static_cast<double>(turso_statement_row_value_int(statement, i)));
          break;
        case TURSO_TYPE_REAL:
          row.emplace_back(turso_statement_row_value_double(statement, i));
          break;
        case TURSO_TYPE_TEXT: {
          auto size = turso_statement_row_value_bytes_count(statement, i);
          auto ptr = turso_statement_row_value_bytes_ptr(statement, i);
          row.emplace_back(std::string(ptr, static_cast<size_t>(size)));
          break;
        }
        case TURSO_TYPE_BLOB: {
          auto size = turso_statement_row_value_bytes_count(statement, i);
          auto ptr = turso_statement_row_value_bytes_ptr(statement, i);
          auto *data = new uint8_t[static_cast<size_t>(size)];
          memcpy(data, ptr, static_cast<size_t>(size));
          row.emplace_back(ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                                       .size = static_cast<size_t>(size)});
          break;
        }
        case TURSO_TYPE_NULL:
        case TURSO_TYPE_UNKNOWN:
        default:
          row.emplace_back(nullptr);
          break;
        }
      }

      rows.emplace_back(std::move(row));
    });

    changes = static_cast<int>(turso_statement_n_change(statement));

    turso_statement_deinit(statement);
  }

  return {.affectedRows = changes,
        .insertId = static_cast<double>(turso_connection_last_insert_rowid(
          require_turso_connection(db_handle, "last_insert_rowid"))),
          .rows = std::move(rows),
          .column_names = std::move(column_names)};
}

BridgeResult opsqlite_execute_host_objects(
    sqlite3 *db, std::string const &query, const std::vector<JSVariant> *params,
    std::vector<DumbHostObject> *results,
    std::shared_ptr<std::vector<SmartHostObject>> &metadatas) {

  auto statement = opsqlite_prepare_statement(db, query);
  if (params != nullptr && !params->empty()) {
    opsqlite_bind_statement(statement, params);
  }

  auto res = opsqlite_execute_prepared_statement(db, statement, results, metadatas);
  opsqlite_finalize_statement(statement);
  return res;
}

BridgeResult opsqlite_execute_raw(
    sqlite3 *db, std::string const &query, const std::vector<JSVariant> *params,
    std::vector<std::vector<JSVariant>> *results) {

  auto response = opsqlite_execute(db, query, params);
  if (results != nullptr) {
    *results = response.rows;
  }

  return {.affectedRows = response.affectedRows,
          .insertId = response.insertId};
}

void opsqlite_register_update_hook([[maybe_unused]] sqlite3 *db,
                                   [[maybe_unused]] void *db_host_object_ptr) {}

void opsqlite_deregister_update_hook([[maybe_unused]] sqlite3 *db) {}

void opsqlite_register_commit_hook([[maybe_unused]] sqlite3 *db,
                                   [[maybe_unused]] void *db_host_object_ptr) {}

void opsqlite_deregister_commit_hook([[maybe_unused]] sqlite3 *db) {}

void opsqlite_register_rollback_hook([[maybe_unused]] sqlite3 *db,
                                     [[maybe_unused]] void *db_host_object_ptr) {}

void opsqlite_deregister_rollback_hook([[maybe_unused]] sqlite3 *db) {}

void opsqlite_load_extension([[maybe_unused]] sqlite3 *db,
                             [[maybe_unused]] std::string &path,
                             [[maybe_unused]] std::string &entry_point) {
  throw std::runtime_error(
      "[op-sqlite][turso] load_extension is not supported by Turso SDK kit backend");
}

BatchResult opsqlite_execute_batch(sqlite3 *db,
                                   const std::vector<BatchArguments> *commands) {
  size_t command_count = commands->size();
  if (command_count == 0) {
    throw std::runtime_error("No SQL commands provided");
  }

  int affected_rows = 0;

  for (size_t i = 0; i < command_count; i++) {
    const auto &command = commands->at(i);
    auto result = opsqlite_execute(db, command.sql, &command.params);
    affected_rows += result.affectedRows;
  }

  return BatchResult{.affectedRows = affected_rows,
                     .commands = static_cast<int>(command_count)};
}

} // namespace opsqlite
