#include "bridge.h"
#include <sstream>
#include <iostream>
#include <sqlite3.h>
#include <ctime>
#include <unistd.h>
#include <sys/stat.h>
#include <map>
#include "logs.h"
#include "DynamicHostObject.h"

namespace osp {

std::unordered_map<std::string, sqlite3 *> dbMap = std::unordered_map<std::string, sqlite3 *>();

bool folder_exists(const std::string &foldername)
{
    struct stat buffer;
    return (stat(foldername.c_str(), &buffer) == 0);
}

/**
 * Portable wrapper for mkdir. Internally used by mkdir()
 * @param[in] path the full path of the directory to create.
 * @return zero on success, otherwise -1.
 */
int _mkdir(const char *path)
{
#if _POSIX_C_SOURCE
    return mkdir(path);
#else
    return mkdir(path, 0755); // not sure if this works on mac
#endif
}

/**
 * Recursive, portable wrapper for mkdir.
 * @param[in] path the full path of the directory to create.
 * @return zero on success, otherwise -1.
 */
int mkdir(const char *path)
{
    std::string current_level = "/";
    std::string level;
    std::stringstream ss(path);
    // First line is empty because it starts with /User
    getline(ss, level, '/');
    // split path using slash as a separator
    while (getline(ss, level, '/'))
    {
        current_level += level; // append folder to the current level
        // create current level
        if (!folder_exists(current_level) && _mkdir(current_level.c_str()) != 0)
            return -1;
        
        current_level += "/"; // don't forget to append a slash
    }
    
    return 0;
}

inline bool file_exists(const std::string &path)
{
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

std::string get_db_path(std::string const dbName, std::string const docPath)
{
    mkdir(docPath.c_str());
    return docPath + "/" + dbName;
}

BridgeResult sqliteOpenDb(std::string const dbName, std::string const docPath)
{
    std::string dbPath = get_db_path(dbName, docPath);
    
    int sqlOpenFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    
    sqlite3 *db;
    int exit = 0;
    exit = sqlite3_open_v2(dbPath.c_str(), &db, sqlOpenFlags, nullptr);
    
    if (exit != SQLITE_OK)
    {
        return {
            .type = SQLiteError,
            .message = sqlite3_errmsg(db)
        };
    }
    else
    {
        dbMap[dbName] = db;
    }
    
    return BridgeResult{
        .type = SQLiteOk,
        .affectedRows = 0
    };
}

BridgeResult sqliteCloseDb(std::string const dbName)
{
    
    if (dbMap.count(dbName) == 0)
    {
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

void sqliteCloseAll() {
    for (auto const& x : dbMap) {
        // In certain cases, this will return SQLITE_OK, mark the database connection as an unusable "zombie",
        // and deallocate the connection later.
        sqlite3_close_v2(x.second);
    }
    dbMap.clear();
}

BridgeResult sqliteAttachDb(std::string const mainDBName, std::string const docPath, std::string const databaseToAttach, std::string const alias)
{
    /**
     * There is no need to check if mainDBName is opened because sqliteExecuteLiteral will do that.
     * */
    std::string dbPath = get_db_path(databaseToAttach, docPath);
    std::string statement = "ATTACH DATABASE '" + dbPath + "' AS " + alias;
    
    BridgeResult result = sqliteExecuteLiteral(mainDBName, statement);
    
    if (result.type == SQLiteError)
    {
        return {
            .type = SQLiteError,
            .message = mainDBName + " was unable to attach another database: " + std::string(result.message),
        };
    }
    return {
        .type = SQLiteOk,
    };
}

BridgeResult sqliteDetachDb(std::string const mainDBName, std::string const alias)
{
    /**
     * There is no need to check if mainDBName is opened because sqliteExecuteLiteral will do that.
     * */
    std::string statement = "DETACH DATABASE " + alias;
    BridgeResult result = sqliteExecuteLiteral(mainDBName, statement);
    if (result.type == SQLiteError)
    {
        return BridgeResult{
            .type = SQLiteError,
            .message = mainDBName + "was unable to detach database: " + std::string(result.message),
        };
    }
    return BridgeResult{
        .type = SQLiteOk,
    };
}

BridgeResult sqliteRemoveDb(std::string const dbName, std::string const docPath)
{
    if (dbMap.count(dbName) == 1)
    {
        BridgeResult closeResult = sqliteCloseDb(dbName);
        if (closeResult.type == SQLiteError)
        {
            return closeResult;
        }
    }
    
    std::string dbPath = get_db_path(dbName, docPath);
    
    if (!file_exists(dbPath))
    {
        return {
            .type = SQLiteError,
            .message = "[op-sqlite]: Database file not found" + dbPath
        };
    }
    
    remove(dbPath.c_str());
    
    return {
        .type = SQLiteOk,
    };
}

void bindStatement(sqlite3_stmt *statement, std::vector<std::any> *values)
{
    size_t size = values->size();
    if (size <= 0)
    {
        return;
    }
    
    for (int ii = 0; ii < size; ii++)
    {
        int sqIndex = ii + 1;
        std::any value = values->at(ii);
        
        if (value.type() == typeid(NULL) || value.type() == typeid(nullptr))
        {
            sqlite3_bind_null(statement, sqIndex);
        }
        else if (value.type() == typeid(bool))
        {
            sqlite3_bind_int(statement, sqIndex, std::any_cast<bool>(value));
        }
        else if (value.type() == typeid(int))
        {
            sqlite3_bind_int(statement, sqIndex, std::any_cast<int>(value));
        }
        else if (value.type() == typeid(long long))
        {
            sqlite3_bind_double(statement, sqIndex, std::any_cast<long long>(value));
        }
        else if (value.type() == typeid(double))
        {
            sqlite3_bind_double(statement, sqIndex, std::any_cast<double>(value));
        }
        else if (value.type() == typeid(std::string))
        {
            std::string str = std::any_cast<std::string>(value);
            sqlite3_bind_text(statement, sqIndex, str.c_str(), str.length(), SQLITE_TRANSIENT);
        } else {
            throw std::invalid_argument("Unsupported scalar type, cannot convert to JSI Value");
        }
//        else if (value.type() == typeid(const char*))
//        {
//            sqlite3_bind_text(statement, sqIndex, str.c_str(), str.length(), SQLITE_TRANSIENT);
//            return jsi::String::createFromAscii(rt, std::any_cast<const char*>(value));
//        }
        // TODO Add support for array buffers
        //        else if (value.isObject())
        //        {
        //            auto obj = value.asObject(rt);
        //            if(obj.isArrayBuffer(rt)) {
        //                auto buf = obj.getArrayBuffer(rt);
        //                sqlite3_bind_blob(statement, sqIndex, buf.data(rt), buf.size(rt), SQLITE_STATIC);
        //            }
        //
//        else if (dataType == ARRAY_BUFFER)
//        {
//            
//        }
//        sqlite3_bind_blob(statement, sqIndex, value.arrayBufferValue.get(), value.arrayBufferSize, SQLITE_STATIC);
        //        }
        
        
        
    }
}

BridgeResult sqliteExecute(std::string const dbName,
                             std::string const &query,
                             std::vector<std::any> *params,
                             std::vector<std::shared_ptr<DynamicHostObject>> *results,
                             std::vector<std::shared_ptr<DynamicHostObject>> *metadatas)
{
    
    if (dbMap.count(dbName) == 0)
    {
        return {
            .type = SQLiteError,
            .message = "[op-sqlite]: Database " + dbName + " is not open",
            .affectedRows = 0
        };
    }
    
    sqlite3 *db = dbMap[dbName];
    
    sqlite3_stmt *statement;
    
    int statementStatus = sqlite3_prepare_v2(db, query.c_str(), -1, &statement, NULL);
    
    if (statementStatus == SQLITE_OK) // statemnet is correct, bind the passed parameters
    {
        bindStatement(statement, params);
    }
    else
    {
        const char *message = sqlite3_errmsg(db);
        return {
            .type = SQLiteError,
            .message = "[op-sqlite] SQL execution error: " + std::string(message),
            .affectedRows = 0};
    }
    
    bool isConsuming = true;
    bool isFailed = false;
    
    int result, i, count, column_type;
    std::string column_name, column_declared_type;
    std::shared_ptr<DynamicHostObject> row;
    
    while (isConsuming)
    {
        result = sqlite3_step(statement);
        
        switch (result)
        {
            case SQLITE_ROW:
                if(results == NULL)
                {
                    break;
                }
                
                i = 0;
                row = std::make_shared<DynamicHostObject>();
                count = sqlite3_column_count(statement);
                
                while (i < count)
                {
                    column_type = sqlite3_column_type(statement, i);
                    column_name = sqlite3_column_name(statement, i);
                    
                    switch (column_type)
                    {
                            
                        case SQLITE_INTEGER:
                        {
                            /**
                             * Warning this will loose precision because JS can
                             * only represent Integers up to 53 bits
                             */
                            double column_value = sqlite3_column_double(statement, i);
                            row->fields[column_name] = std::any(column_value);
                            break;
                        }
                            
                        case SQLITE_FLOAT:
                        {
                            double column_value = sqlite3_column_double(statement, i);
                            row->fields[column_name] = std::any(column_value);
                            break;
                        }
                            
                        case SQLITE_TEXT:
                        {
                            const char *column_value = reinterpret_cast<const char *>(sqlite3_column_text(statement, i));
                            int byteLen = sqlite3_column_bytes(statement, i);
                            // Specify length too; in case string contains NULL in the middle (which SQLite supports!)
                            row->fields[column_name] = std::any(std::string(column_value, byteLen));
                            break;
                        }
                            
                        case SQLITE_BLOB:
                        {
                            int blob_size = sqlite3_column_bytes(statement, i);
                            const void *blob = sqlite3_column_blob(statement, i);
                            uint8_t *data = new uint8_t[blob_size];
                            memcpy(data, blob, blob_size);
                            // TODO array buffer support
                            //                            row[column_name] = createArrayBufferQuickValue(data, blob_size);
                            break;
                        }
                            
                        case SQLITE_NULL:
                            // Intentionally left blank

                        default:
                            row->fields[column_name] = std::any(NULL);
                            break;
                    }
                    i++;
                }
                results->push_back(row);
                break;

            case SQLITE_DONE:
                if(metadatas != nullptr)
                {
                    i = 0;
                    count = sqlite3_column_count(statement);
                    while (i < count)
                    {
                        column_name = sqlite3_column_name(statement, i);
                        const char *type = sqlite3_column_decltype(statement, i);
                        auto metadata = std::make_shared<DynamicHostObject>();
                        metadata->fields["name"] = column_name;
                        metadata->fields["index"] = i;
                        metadata->fields["type"] = type;
                        
                        i++;
                    }
                }
                isConsuming = false;
                break;
                
            default:
                isFailed = true;
                isConsuming = false;
        }
    }
    
    sqlite3_finalize(statement);
    
    if (isFailed)
    {
        const char *message = sqlite3_errmsg(db);
        return {
            .type = SQLiteError,
            .message = "[op-sqlite] SQL execution error: " + std::string(message),
            .affectedRows = 0,
            .insertId = 0
        };
    }
    
    int changedRowCount = sqlite3_changes(db);
    long long latestInsertRowId = sqlite3_last_insert_rowid(db);
    
    return {
        .type = SQLiteOk,
        .affectedRows = changedRowCount,
        .insertId = static_cast<double>(latestInsertRowId)};
}

BridgeResult sqliteExecuteLiteral(std::string const dbName, std::string const &query)
{
    // Check if db connection is opened
    if (dbMap.count(dbName) == 0)
    {
        return {
            SQLiteError,
            "[op-sqlite] Database not opened: " + dbName
        };
    }
    
    sqlite3 *db = dbMap[dbName];
    sqlite3_stmt *statement;
    
    // Compile and move result into statement memory spot
    int statementStatus = sqlite3_prepare_v2(db, query.c_str(), -1, &statement, NULL);
    
    if (statementStatus != SQLITE_OK)
    {
        const char *message = sqlite3_errmsg(db);
        return {
            SQLiteError,
            "[op-sqlite] SQL execution error: " + std::string(message),
            0};
    }
    
    bool isConsuming = true;
    bool isFailed = false;
    
    int result;
    std::string column_name;
    
    while (isConsuming)
    {
        result = sqlite3_step(statement);
        
        switch (result)
        {
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
    
    if (isFailed)
    {
        const char *message = sqlite3_errmsg(db);
        return {
            SQLiteError,
            "[op-sqlite] SQL execution error: " + std::string(message),
            0};
    }
    
    int changedRowCount = sqlite3_changes(db);
    return {
        SQLiteOk,
        "",
        changedRowCount};
}

}
