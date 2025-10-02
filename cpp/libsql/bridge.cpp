#include "bridge.h"
#include "DumbHostObject.h"
#include "SmartHostObject.h"
#include "libsql.h"
#include "logs.h"
#include "utils.h"
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <variant>

namespace opsqlite {

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

    // Will return false if the directory already exists, no need to check
    std::filesystem::create_directories(location);

    if (!location.empty() && location.back() != '/') {
        return location + "/" + db_name;
    }

    return location + db_name;
}

DB opsqlite_libsql_open_sync(std::string const &name,
                             std::string const &base_path,
                             std::string const &url,
                             std::string const &auth_token, int sync_interval,
                             bool offline, std::string const &encryption_key,
                             std::string const &remote_encryption_key) {
    std::string path = opsqlite_get_db_path(name, base_path);

    int status;
    libsql_database_t db;
    libsql_connection_t c;
    const char *err = nullptr;

    libsql_config config = {
        .db_path = path.c_str(),
        .primary_url = url.c_str(),
        .auth_token = auth_token.c_str(),
        .read_your_writes = '1',
        .encryption_key =
            encryption_key.empty() ? nullptr : encryption_key.c_str(),
        .remote_encryption_key = remote_encryption_key.empty()
                                     ? nullptr
                                     : remote_encryption_key.c_str(),
        .sync_interval = sync_interval,
        .with_webpki = '1',
        .offline = offline,
    };

    status = libsql_open_sync_with_config(config, &db, &err);
    if (status != 0) {
        throw std::runtime_error(err);
    }

    status = libsql_connect(db, &c, &err);

    if (status != 0) {
        throw std::runtime_error(err);
    }

    return {.db = db, .c = c};
}

DB opsqlite_libsql_open(std::string const &name, std::string const &last_path,
                        std::string const &crsqlitePath) {
    std::string path = opsqlite_get_db_path(name, last_path);

    int status;
    libsql_database_t db;
    libsql_connection_t c;
    const char *err = nullptr;

    status = libsql_open_file(path.c_str(), &db, &err);

    if (status != 0) {
        throw std::runtime_error(err);
    }

    status = libsql_connect(db, &c, &err);

    if (status != 0) {
        throw std::runtime_error(err);
    }

#ifdef OP_SQLITE_USE_CRSQLITE
    const char *errMsg;
    const char *crsqliteEntryPoint = "sqlite3_crsqlite_init";

    status = libsql_load_extension(c, crsqlitePath.c_str(), crsqliteEntryPoint,
                                   &errMsg);

    if (status != 0) {
        throw std::runtime_error(errMsg);
    } else {
        LOGI("Loaded CRSQlite successfully");
    }
#endif

    return {.db = db, .c = c};
}

DB opsqlite_libsql_open_remote(std::string const &url,
                               std::string const &auth_token) {
    int status;
    libsql_database_t db;
    libsql_connection_t c;
    const char *err = nullptr;

    status = libsql_open_remote_with_webpki(url.c_str(), auth_token.c_str(),
                                            &db, &err);

    if (status != 0) {
        throw std::runtime_error(err);
    }

    status = libsql_connect(db, &c, &err);

    if (status != 0) {
        throw std::runtime_error(err);
    }

    return {.db = db, .c = c};
}

void opsqlite_libsql_close(DB &db) {
    if (db.c != nullptr) {
        libsql_disconnect(db.c);
        db.c = nullptr;
    }
    if (db.db != nullptr) {
        libsql_close(db.db);
        db.db = nullptr;
    }
}

void opsqlite_libsql_attach(DB const &db, std::string const &docPath,
                            std::string const &databaseToAttach,
                            std::string const &alias) {
    std::string dbPath = opsqlite_get_db_path(databaseToAttach, docPath);
    std::string statement = "ATTACH DATABASE '" + dbPath + "' AS " + alias;

    opsqlite_libsql_execute(db, statement, nullptr);
}

void opsqlite_libsql_detach(DB const &db, std::string const &alias) {
    std::string statement = "DETACH DATABASE " + alias;
    opsqlite_libsql_execute(db, statement, nullptr);
}

void opsqlite_libsql_sync(DB const &db) {
    const char *err = nullptr;

    int status = libsql_sync(db.db, &err);

    if (status != 0) {
        throw std::runtime_error(err);
    }
}

void opsqlite_libsql_remove(DB &db, std::string const &name,
                            std::string const &path) {
    opsqlite_libsql_close(db);

    std::string full_path = opsqlite_get_db_path(name, path);

    if (!file_exists(full_path)) {
        throw std::runtime_error("[op-sqlite]: Database file not found" +
                                 full_path);
    }

    remove(full_path.c_str());
}

void opsqlite_libsql_bind_statement(libsql_stmt_t statement,
                                    const std::vector<JSVariant> *values) {
    const char *err;
    size_t size = values->size();

    for (int ii = 0; ii < size; ii++) {
        int index = ii + 1;
        JSVariant value = values->at(ii);
        int status;

        if (std::holds_alternative<bool>(value)) {
            status =
                libsql_bind_int(statement, index,
                                static_cast<int>(std::get<bool>(value)), &err);
        } else if (std::holds_alternative<int>(value)) {
            status =
                libsql_bind_int(statement, index, std::get<int>(value), &err);
        } else if (std::holds_alternative<long long>(value)) {
            status = libsql_bind_int(statement, index,
                                     std::get<long long>(value), &err);
        } else if (std::holds_alternative<double>(value)) {
            status = libsql_bind_float(statement, index,
                                       std::get<double>(value), &err);
        } else if (std::holds_alternative<std::string>(value)) {
            std::string str = std::get<std::string>(value);
            status = libsql_bind_string(statement, index, str.c_str(), &err);
        } else if (std::holds_alternative<ArrayBuffer>(value)) {
            ArrayBuffer buffer = std::get<ArrayBuffer>(value);
            status = libsql_bind_blob(statement, index, buffer.data.get(),
                                      static_cast<int>(buffer.size), &err);
        } else {
            status = libsql_bind_null(statement, index, &err);
        }

        if (status != 0) {
            throw std::runtime_error(err);
        }
    }
}

BridgeResult opsqlite_libsql_execute_prepared_statement(
    DB const &db, libsql_stmt_t stmt, std::vector<DumbHostObject> *results,
    const std::shared_ptr<std::vector<SmartHostObject>> &metadatas) {

    libsql_rows_t rows;
    libsql_row_t row;

    int status;
    const char *err = nullptr;

    status = libsql_query_stmt(stmt, &rows, &err);

    if (status != 0) {
        throw std::runtime_error(err);
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
                row_host_object.values.emplace_back(int_value);
                break;

            case LIBSQL_FLOAT:
                double float_value;
                status = libsql_get_float(row, col, &float_value, &err);
                row_host_object.values.emplace_back(float_value);
                break;

            case LIBSQL_TEXT:
                const char *text_value;
                status = libsql_get_string(row, col, &text_value, &err);
                row_host_object.values.emplace_back(text_value);
                break;

            case LIBSQL_BLOB: {
                blob value_blob;
                libsql_get_blob(row, col, &value_blob, &err);
                auto *data = new uint8_t[value_blob.len];
                // You cannot share raw memory between native and JS
                // always copy the data
                memcpy(data, value_blob.ptr, value_blob.len);
                libsql_free_blob(value_blob);
                row_host_object.values.emplace_back(
                    ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                                .size = static_cast<size_t>(value_blob.len)});
                break;
            }

            case LIBSQL_NULL:
                // intentional fall-through
            default:
                row_host_object.values.emplace_back(nullptr);
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
                metadata.fields.emplace_back("name", col_name);
                metadata.fields.emplace_back("index", col);
                metadata.fields.emplace_back("type", "UNKNOWN");
                //                  metadata.fields.push_back(
                //                      std::make_pair("type", type == -1 ?
                //                      "UNKNOWN" : type));

                metadatas->push_back(metadata);
            }
        }

        if (results != nullptr) {
            results->push_back(row_host_object);
        }

        metadata_set = true;
        err = nullptr;
    }

    if (status != 0) {
        fprintf(stderr, "%s\n", err);
    }

    libsql_free_rows(rows);

    unsigned long long changes = libsql_changes(db.c);
    long long insert_row_id = libsql_last_insert_rowid(db.c);

    libsql_reset_stmt(stmt, &err);

    return {.affectedRows = static_cast<int>(changes),
            .insertId = static_cast<double>(insert_row_id)};
}

libsql_stmt_t opsqlite_libsql_prepare_statement(DB const &db,
                                                std::string const &query) {
    libsql_stmt_t stmt;

    const char *err;

    int status = libsql_prepare(db.c, query.c_str(), &stmt, &err);

    if (status != 0) {
        throw std::runtime_error(err);
    }

    return stmt;
}

BridgeResult opsqlite_libsql_execute(DB const &db, std::string const &query,
                                     const std::vector<JSVariant> *params) {

    std::vector<std::string> column_names;
    std::vector<std::vector<JSVariant>> out_rows;
    std::vector<JSVariant> out_row;
    libsql_rows_t rows;
    libsql_row_t row;
    libsql_stmt_t stmt;
    int status;
    const char *err = nullptr;

    status = libsql_prepare(db.c, query.c_str(), &stmt, &err);

    if (status != 0) {
        throw std::runtime_error(err);
    }

    if (params != nullptr && !params->empty()) {
        opsqlite_libsql_bind_statement(stmt, params);
    }

    status = libsql_query_stmt(stmt, &rows, &err);

    if (status != 0) {
        throw std::runtime_error(err);
    }

    // Get the column names on the first pass
    int column_count = libsql_column_count(rows);
    const char *col_name;

    for (int i = 0; i < column_count; i++) {
        status = libsql_column_name(rows, i, &col_name, &err);
        if (status != 0) {
            throw std::runtime_error(err);
        }
        column_names.emplace_back(col_name);
    }

    long long int_value;
    double float_value;
    const char *text_value;
    blob blob_value;

    status = libsql_next_row(rows, &row, &err);
    while (status == 0) {
        out_row = std::vector<JSVariant>();

        if (!err && !row) {
            break;
        }

        for (int col = 0; col < column_count; col++) {
            int type;

            libsql_column_type(rows, row, col, &type, &err);

            switch (type) {
            case LIBSQL_INT:
                status = libsql_get_int(row, col, &int_value, &err);
                out_row.emplace_back(int_value);
                break;

            case LIBSQL_FLOAT:
                status = libsql_get_float(row, col, &float_value, &err);
                out_row.emplace_back(float_value);
                break;

            case LIBSQL_TEXT:
                status = libsql_get_string(row, col, &text_value, &err);
                out_row.emplace_back(text_value);
                break;

            case LIBSQL_BLOB: {
                libsql_get_blob(row, col, &blob_value, &err);
                auto data = new uint8_t[blob_value.len];
                // You cannot share raw memory between native and JS
                // always copy the data
                memcpy(data, blob_value.ptr, blob_value.len);
                libsql_free_blob(blob_value);
                out_row.emplace_back(
                    ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                                .size = static_cast<size_t>(blob_value.len)});
                break;
            }

            case LIBSQL_NULL:
                // intentional fall-through
            default:
                out_row.emplace_back(nullptr);
                break;
            }

            if (status != 0) {
                throw std::runtime_error(err);
            }
        }

        out_rows.emplace_back(out_row);
        err = nullptr;
        status = libsql_next_row(rows, &row, &err);
    }

    libsql_free_rows(rows);
    libsql_free_stmt(stmt);

    unsigned long long changes = libsql_changes(db.c);
    long long insert_row_id = libsql_last_insert_rowid(db.c);

    return {.affectedRows = static_cast<int>(changes),
            .insertId = static_cast<double>(insert_row_id),
            .rows = std::move(out_rows),
            .column_names = std::move(column_names)};
}

BridgeResult opsqlite_libsql_execute_with_host_objects(
    DB const &db, std::string const &query,
    const std::vector<JSVariant> *params, std::vector<DumbHostObject> *results,
    const std::shared_ptr<std::vector<SmartHostObject>> &metadatas) {

    libsql_rows_t rows;
    libsql_row_t row;
    libsql_stmt_t stmt;
    int status;
    const char *err = nullptr;

    status = libsql_prepare(db.c, query.c_str(), &stmt, &err);

    if (status != 0) {
        throw std::runtime_error(err);
    }

    if (params != nullptr && !params->empty()) {
        opsqlite_libsql_bind_statement(stmt, params);
    }

    status = libsql_query_stmt(stmt, &rows, &err);

    if (status != 0) {
        throw std::runtime_error(err);
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
                row_host_object.values.emplace_back(int_value);
                break;

            case LIBSQL_FLOAT:
                double float_value;
                status = libsql_get_float(row, col, &float_value, &err);
                row_host_object.values.emplace_back(float_value);
                break;

            case LIBSQL_TEXT:
                const char *text_value;
                status = libsql_get_string(row, col, &text_value, &err);
                row_host_object.values.emplace_back(text_value);
                break;

            case LIBSQL_BLOB: {
                blob value_blob;
                libsql_get_blob(row, col, &value_blob, &err);
                auto *data = new uint8_t[value_blob.len];
                // You cannot share raw memory between native and JS
                // always copy the data
                memcpy(data, value_blob.ptr, value_blob.len);
                libsql_free_blob(value_blob);
                row_host_object.values.emplace_back(
                    ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                                .size = static_cast<size_t>(value_blob.len)});
                break;
            }

            case LIBSQL_NULL:
                // intentional fall-through
            default:
                row_host_object.values.emplace_back(nullptr);
                break;
            }

            if (status != 0) {
                fprintf(stderr, "%s\n", err);
                throw std::runtime_error(err);
            }

            // On the first interation through the columns, set the metadata
            if (!metadata_set && metadatas != nullptr) {
                const char *col_name;
                status = libsql_column_name(rows, col, &col_name, &err);

                auto metadata = SmartHostObject();
                metadata.fields.emplace_back("name", col_name);
                metadata.fields.emplace_back("index", col);
                metadata.fields.emplace_back("type", "UNKNOWN");
                //                  metadata.fields.push_back(
                //                      std::make_pair("type", type == -1 ?
                //                      "UNKNOWN" : type));

                metadatas->push_back(metadata);
            }
        }

        if (results != nullptr) {
            results->push_back(row_host_object);
        }

        metadata_set = true;
        err = nullptr;
    }

    if (status != 0) {
        fprintf(stderr, "%s\n", err);
    }

    libsql_free_rows(rows);
    libsql_free_stmt(stmt);

    unsigned long long changes = libsql_changes(db.c);
    long long insert_row_id = libsql_last_insert_rowid(db.c);

    return {.affectedRows = static_cast<int>(changes),
            .insertId = static_cast<double>(insert_row_id)};
}

/// Executes returning data in raw arrays, a small performance optimization
/// for certain use cases
BridgeResult
opsqlite_libsql_execute_raw(DB const &db, std::string const &query,
                            const std::vector<JSVariant> *params,
                            std::vector<std::vector<JSVariant>> *results) {

    libsql_rows_t rows;
    libsql_row_t row;
    libsql_stmt_t stmt;
    int status;
    const char *err = nullptr;

    status = libsql_prepare(db.c, query.c_str(), &stmt, &err);

    if (status != 0) {
        throw std::runtime_error(err);
    }

    if (params != nullptr && !params->empty()) {
        opsqlite_libsql_bind_statement(stmt, params);
    }

    status = libsql_query_stmt(stmt, &rows, &err);

    if (status != 0) {
        throw std::runtime_error(err);
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
                row_vector.emplace_back(int_value);
                break;

            case LIBSQL_FLOAT:
                double float_value;
                status = libsql_get_float(row, col, &float_value, &err);
                row_vector.emplace_back(float_value);
                break;

            case LIBSQL_TEXT:
                const char *text_value;
                status = libsql_get_string(row, col, &text_value, &err);
                row_vector.emplace_back(text_value);
                break;

            case LIBSQL_BLOB: {
                blob value_blob;
                libsql_get_blob(row, col, &value_blob, &err);
                auto *data = new uint8_t[value_blob.len];
                // You cannot share raw memory between native and JS
                // always copy the data
                memcpy(data, value_blob.ptr, value_blob.len);
                libsql_free_blob(value_blob);
                row_vector.emplace_back(
                    ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                                .size = static_cast<size_t>(value_blob.len)});
                break;
            }

            case LIBSQL_NULL:
                // intentional fall-through
            default:
                row_vector.emplace_back(nullptr);
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

        err = nullptr;
    }

    if (status != 0) {
        fprintf(stderr, "%s\n", err);
    }

    libsql_free_rows(rows);
    libsql_free_stmt(stmt);

    unsigned long long changes = libsql_changes(db.c);
    long long insert_row_id = libsql_last_insert_rowid(db.c);

    return {.affectedRows = static_cast<int>(changes),
            .insertId = static_cast<double>(insert_row_id)};
}

BatchResult
opsqlite_libsql_execute_batch(DB const &db,
                              const std::vector<BatchArguments> *commands) {
    size_t commandCount = commands->size();
    if (commandCount <= 0) {
        throw std::runtime_error("No SQL commands provided");
    }

    try {
        int affectedRows = 0;
        // opsqlite_libsql_execute(db, "BEGIN EXCLUSIVE TRANSACTION", nullptr);
        for (int i = 0; i < commandCount; i++) {
            auto command = commands->at(i);
            // We do not provide a datastructure to receive query data because
            // we don't need/want to handle this results in a batch execution
            auto result =
                opsqlite_libsql_execute(db, command.sql, &command.params);
            affectedRows += result.affectedRows;
        }
        // opsqlite_libsql_execute(db, "COMMIT", nullptr);
        return BatchResult{
            .affectedRows = affectedRows,
            .commands = static_cast<int>(commandCount),
        };
    } catch (std::exception &exc) {
        // opsqlite_libsql_execute(db, "ROLLBACK", nullptr);
        return BatchResult{
            .message = exc.what(),
        };
    }
}

} // namespace opsqlite
