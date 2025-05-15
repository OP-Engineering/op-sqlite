#include "DBHostObject.h"
#include "PreparedStatementHostObject.h"
#if OP_SQLITE_USE_LIBSQL
#include "libsql/bridge.h"
#else
#include "bridge.h"
#endif
#include "logs.h"
#include "macros.h"
#include "utils.h"
#include <iostream>
#include <utility>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

#ifdef OP_SQLITE_USE_LIBSQL
void DBHostObject::flush_pending_reactive_queries(
    const std::shared_ptr<jsi::Value> &resolve) {
    invoker->invokeAsync([this, resolve]() {
        resolve->asObject(rt).asFunction(rt).call(rt, {});
    });
}
#else
void DBHostObject::flush_pending_reactive_queries(
    const std::shared_ptr<jsi::Value> &resolve) {
    for (const auto &query_ptr : pending_reactive_queries) {
        auto query = query_ptr.get();

        std::vector<DumbHostObject> results;
        std::shared_ptr<std::vector<SmartHostObject>> metadata =
            std::make_shared<std::vector<SmartHostObject>>();

        auto status = opsqlite_execute_prepared_statement(db, query->stmt,
                                                          &results, metadata);

        invoker->invokeAsync(
            [this,
             results = std::make_shared<std::vector<DumbHostObject>>(results),
             callback = query->callback, metadata, status = std::move(status)] {
                auto jsiResult =
                    create_result(rt, status, results.get(), metadata);
                callback->asObject(rt).asFunction(rt).call(rt, jsiResult);
            });
    }

    pending_reactive_queries.clear();

    invoker->invokeAsync([this, resolve]() {
        resolve->asObject(rt).asFunction(rt).call(rt, {});
    });
}

void DBHostObject::on_commit() {
    invoker->invokeAsync(
        [this] { commit_hook_callback->asObject(rt).asFunction(rt).call(rt); });
}

void DBHostObject::on_rollback() {
    invoker->invokeAsync([this] {
        rollback_hook_callback->asObject(rt).asFunction(rt).call(rt);
    });
}

void DBHostObject::on_update(const std::string &table,
                             const std::string &operation, long long row_id) {
    if (update_hook_callback != nullptr) {
        invoker->invokeAsync(
            [this, callback = update_hook_callback, table, operation, row_id] {
                auto res = jsi::Object(rt);
                res.setProperty(rt, "table",
                                jsi::String::createFromUtf8(rt, table));
                res.setProperty(rt, "operation",
                                jsi::String::createFromUtf8(rt, operation));
                res.setProperty(rt, "rowId",
                                jsi::Value(static_cast<double>(row_id)));

                callback->asObject(rt).asFunction(rt).call(rt, res);
            });
    }

    for (const auto &query_ptr : reactive_queries) {
        auto query = query_ptr.get();
        if (query->discriminators.empty()) {
            continue;
        }

        bool shouldFire = false;

        for (const auto &discriminator : query->discriminators) {
            // Tables don't match then skip
            if (discriminator.table != table) {
                continue;
            }

            // If no ids are specified, then we should fire
            if (discriminator.ids.empty()) {
                shouldFire = true;
                break;
            }

            // If ids are specified, then we should check if the rowId matches
            for (const auto &discrimator_id : discriminator.ids) {
                if (row_id == discrimator_id) {
                    shouldFire = true;
                    break;
                }
            }
        }

        if (shouldFire) {
            pending_reactive_queries.insert(query_ptr);
        }
    }
}

void DBHostObject::auto_register_update_hook() {
    if (update_hook_callback == nullptr && reactive_queries.empty() &&
        is_update_hook_registered) {
        opsqlite_deregister_update_hook(db);
        is_update_hook_registered = false;
        return;
    }

    if (is_update_hook_registered) {
        return;
    }

    opsqlite_register_update_hook(db, this);
    is_update_hook_registered = true;
}
#endif

//    _____                _                   _
//   / ____|              | |                 | |
//  | |     ___  _ __  ___| |_ _ __ _   _  ___| |_ ___  _ __
//  | |    / _ \| '_ \/ __| __| '__| | | |/ __| __/ _ \| '__|
//  | |___| (_) | | | \__ \ |_| |  | |_| | (__| || (_) | |
//   \_____\___/|_| |_|___/\__|_|   \__,_|\___|\__\___/|_|
#ifdef OP_SQLITE_USE_LIBSQL
DBHostObject::DBHostObject(jsi::Runtime &rt, std::string &url,
                           std::string &auth_token,
                           std::shared_ptr<react::CallInvoker> invoker)
    : db_name(url), invoker(std::move(invoker)), rt(rt) {
    _thread_pool = std::make_shared<ThreadPool>();
    db = opsqlite_libsql_open_remote(url, auth_token);

    create_jsi_functions();
}

DBHostObject::DBHostObject(jsi::Runtime &rt,
                           std::shared_ptr<react::CallInvoker> invoker,
                           std::string &db_name, std::string &path,
                           std::string &url, std::string &auth_token,
                           int sync_interval, bool offline)
    : db_name(db_name), invoker(std::move(invoker)), rt(rt) {
    _thread_pool = std::make_shared<ThreadPool>();
    db = opsqlite_libsql_open_sync(db_name, path, url, auth_token,
                                   sync_interval, offline);

    create_jsi_functions();
}

#endif

DBHostObject::DBHostObject(jsi::Runtime &rt, std::string &base_path,
                           std::shared_ptr<react::CallInvoker> invoker,
                           std::string &db_name, std::string &path,
                           std::string &crsqlite_path,
                           std::string &sqlite_vec_path,
                           std::string &encryption_key)
    : base_path(base_path), invoker(std::move(invoker)), db_name(db_name),
      rt(rt) {
    _thread_pool = std::make_shared<ThreadPool>();

#ifdef OP_SQLITE_USE_SQLCIPHER
    db = opsqlite_open(db_name, path, crsqlite_path, sqlite_vec_path,
                       encryption_key);
#elif OP_SQLITE_USE_LIBSQL
    db = opsqlite_libsql_open(db_name, path, crsqlite_path);
#else
    db = opsqlite_open(db_name, path, crsqlite_path, sqlite_vec_path);
#endif
    create_jsi_functions();
};

void DBHostObject::create_jsi_functions() {
    function_map["attach"] = HOSTFN("attach") {
        std::string secondary_db_path = std::string(base_path);

        auto obj_params = args[0].asObject(rt);

        std::string secondary_db_name =
            obj_params.getProperty(rt, "secondaryDbFileName")
                .asString(rt)
                .utf8(rt);
        std::string alias =
            obj_params.getProperty(rt, "alias").asString(rt).utf8(rt);

        if (obj_params.hasProperty(rt, "location")) {
            std::string location =
                obj_params.getProperty(rt, "location").asString(rt).utf8(rt);
            secondary_db_path = secondary_db_path + location;
        }

#ifdef OP_SQLITE_USE_LIBSQL
        opsqlite_libsql_attach(db, secondary_db_path, secondary_db_name, alias);
#else
        opsqlite_attach(db, secondary_db_path, secondary_db_name, alias);
#endif

        return {};
    });

    function_map["detach"] = HOSTFN("detach") {

        if (!args[0].isString()) {
            throw std::runtime_error("[op-sqlite] alias must be a strings");
        }

        std::string alias = args[0].asString(rt).utf8(rt);
#ifdef OP_SQLITE_USE_LIBSQL
        opsqlite_libsql_detach(db, alias);
#else
        opsqlite_detach(db, alias);
#endif

        return {};
    });

    function_map["close"] = HOSTFN("close") {
        invalidated = true;

#ifdef OP_SQLITE_USE_LIBSQL
        opsqlite_libsql_close(db);
#else
        opsqlite_close(db);
#endif

        return {};
    });

    function_map["delete"] = HOSTFN("delete") {
        invalidated = true;

        std::string path = std::string(base_path);

        if (count == 1) {
            if (!args[1].isString()) {
                throw std::runtime_error(
                    "[op-sqlite][open] database location must be a string");
            }

            std::string location = args[1].asString(rt).utf8(rt);

            if (!location.empty()) {
                if (location == ":memory:") {
                    path = ":memory:";
                } else if (location.rfind('/', 0) == 0) {
                    path = location;
                } else {
                    path = path + "/" + location;
                }
            }
        }

#ifdef OP_SQLITE_USE_LIBSQL
        opsqlite_libsql_remove(db, db_name, path);
#else
        opsqlite_remove(db, db_name, path);
#endif

        return {};
    });

    function_map["executeRaw"] = HOSTFN("executeRaw") {
        const std::string query = args[0].asString(rt).utf8(rt);
        std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                            ? to_variant_vec(rt, args[1])
                                            : std::vector<JSVariant>();

        auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
    auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor") {
            auto resolve = std::make_shared<jsi::Value>(rt, args[0]);
            auto reject = std::make_shared<jsi::Value>(rt, args[1]);

            auto task = [this, &rt, query, params, resolve, reject]() {
                try {
                    std::vector<std::vector<JSVariant>> results;

#ifdef OP_SQLITE_USE_LIBSQL
                    auto status = opsqlite_libsql_execute_raw(
                        db, query, &params, &results);
#else
                    auto status =
                        opsqlite_execute_raw(db, query, &params, &results);
#endif

                    if (invalidated) {
                        return;
                    }

                    invoker->invokeAsync([&rt, results = std::move(results),
                                          status = std::move(status), resolve,
                                          reject] {
                        auto jsiResult =
                            create_raw_result(rt, status, &results);
                        resolve->asObject(rt).asFunction(rt).call(
                            rt, std::move(jsiResult));
                    });
                } catch (std::runtime_error &e) {
                    auto what = e.what();
                    invoker->invokeAsync([&rt, what, reject] {
                        auto errorCtr =
                            rt.global().getPropertyAsFunction(rt, "Error");
                        auto error = errorCtr.callAsConstructor(
                            rt, jsi::String::createFromAscii(rt, what));
                        reject->asObject(rt).asFunction(rt).call(rt, error);
                    });
                } catch (std::exception &exc) {
                    auto what = exc.what();
                    invoker->invokeAsync([&rt, what, reject] {
                        auto errorCtr =
                            rt.global().getPropertyAsFunction(rt, "Error");
                        auto error = errorCtr.callAsConstructor(
                            rt, jsi::String::createFromAscii(rt, what));
                        reject->asObject(rt).asFunction(rt).call(rt, error);
                    });
                }
            };

            _thread_pool->queueWork(task);

            return {};
     }));

    return promise;
    });

    function_map["executeSync"] = HOSTFN("executeSync") {
        std::string query = args[0].asString(rt).utf8(rt);
        std::vector<JSVariant> params;

        if (count == 2) {
            params = to_variant_vec(rt, args[1]);
        }
#ifdef OP_SQLITE_USE_LIBSQL
        auto status = opsqlite_libsql_execute(db, query, &params);
#else
        auto status = opsqlite_execute(db, query, &params);
#endif

        return create_js_rows(rt, status);
    });

    function_map["execute"] = HOSTFN("execute") {
        const std::string query = args[0].asString(rt).utf8(rt);
        std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                            ? to_variant_vec(rt, args[1])
                                            : std::vector<JSVariant>();

        auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
            auto promise = promiseCtr.callAsConstructor(rt,
 HOSTFN("executor") {
            auto task = [this, &rt, query, params,
                         resolve = std::make_shared<jsi::Value>(rt, args[0]),
                         reject = std::make_shared<jsi::Value>(rt, args[1])]() {
                try {
#ifdef OP_SQLITE_USE_LIBSQL
                    auto status = opsqlite_libsql_execute(db, query, &params);
#else
                    auto status = opsqlite_execute(db, query, &params);
#endif

                    if (invalidated) {
                        return;
                    }

                    invoker->invokeAsync(
                        [&rt, status = std::move(status), resolve, reject] {
                            auto jsiResult = create_js_rows(rt, status);
                            resolve->asObject(rt).asFunction(rt).call(
                                rt, std::move(jsiResult));
                        });
                    // On Android RN is broken and does not correctly match
                    // runtime_error to the generic exception We have to
                    // explicitly catch it
                    // https://github.com/facebook/react-native/issues/48027
                } catch (std::runtime_error &e) {
                    auto what = e.what();
                    invoker->invokeAsync(
                        [&rt, what = std::string(what), reject] {
                            auto errorCtr =
                                rt.global().getPropertyAsFunction(rt, "Error");
                            auto error = errorCtr.callAsConstructor(
                                rt, jsi::String::createFromAscii(rt, what));
                            reject->asObject(rt).asFunction(rt).call(rt, error);
                        });
                } catch (std::exception &exc) {
                    auto what = exc.what();
                    invoker->invokeAsync(
                        [&rt, what = std::string(what), reject] {
                            auto errorCtr =
                                rt.global().getPropertyAsFunction(rt, "Error");
                            auto error = errorCtr.callAsConstructor(
                                rt, jsi::String::createFromAscii(rt, what));
                            reject->asObject(rt).asFunction(rt).call(rt, error);
                        });
                }
            };

            _thread_pool->queueWork(task);

            return {};
    }));

            return promise;
    });

    function_map["executeWithHostObjects"] = HOSTFN("executeWithHostObjects") {
        const std::string query = args[0].asString(rt).utf8(rt);
        std::vector<JSVariant> params;

        if (count == 2) {
            const jsi::Value &originalParams = args[1];
            params = to_variant_vec(rt, originalParams);
        }

        auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
    auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor") {
            auto resolve = std::make_shared<jsi::Value>(rt, args[0]);
            auto reject = std::make_shared<jsi::Value>(rt, args[1]);

            auto task = [&rt, this, query, params, resolve, reject]() {
                try {
                    std::vector<DumbHostObject> results;
                    std::shared_ptr<std::vector<SmartHostObject>> metadata =
                        std::make_shared<std::vector<SmartHostObject>>();
#ifdef OP_SQLITE_USE_LIBSQL
                    auto status = opsqlite_libsql_execute_with_host_objects(
                        db, query, &params, &results, metadata);
#else
                    auto status = opsqlite_execute_host_objects(
                        db, query, &params, &results, metadata);
#endif

                    if (invalidated) {
                        return;
                    }

                    invoker->invokeAsync(
                        [&rt,
                         results =
                             std::make_shared<std::vector<DumbHostObject>>(
                                 results),
                         metadata, status = std::move(status), resolve,
                         reject] {
                            auto jsiResult = create_result(
                                rt, status, results.get(), metadata);
                            resolve->asObject(rt).asFunction(rt).call(
                                rt, std::move(jsiResult));
                        });
                } catch (std::runtime_error &e) {
                    auto what = e.what();
                    invoker->invokeAsync(
                        [&rt, what = std::string(what), reject] {
                            auto errorCtr =
                                rt.global().getPropertyAsFunction(rt, "Error");
                            auto error = errorCtr.callAsConstructor(
                                rt, jsi::String::createFromAscii(rt, what));
                            reject->asObject(rt).asFunction(rt).call(rt, error);
                        });
                } catch (std::exception &exc) {
                    auto what = exc.what();
                    invoker->invokeAsync([&rt, what, reject] {
                        auto errorCtr =
                            rt.global().getPropertyAsFunction(rt, "Error");
                        auto error = errorCtr.callAsConstructor(
                            rt, jsi::String::createFromAscii(rt, what));
                        reject->asObject(rt).asFunction(rt).call(rt, error);
                    });
                }
            };

            _thread_pool->queueWork(task);

            return {};
      }));

    return promise;
    });

    function_map["executeBatch"] = HOSTFN("executeBatch") {
        if (count < 1) {
            throw std::runtime_error(
                "[op-sqlite][executeAsyncBatch] Incorrect parameter count");
        }

        const jsi::Value &params = args[0];

        if (params.isNull() || params.isUndefined()) {
            throw std::runtime_error(
                "[op-sqlite][executeAsyncBatch] - An array of SQL "
                "commands or parameters is needed");
        }

        const jsi::Array &batchParams = params.asObject(rt).asArray(rt);

        std::vector<BatchArguments> commands;
        to_batch_arguments(rt, batchParams, &commands);

        auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
            auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor") {
            auto resolve = std::make_shared<jsi::Value>(rt, args[0]);
            auto reject = std::make_shared<jsi::Value>(rt, args[1]);

            auto task = [this, &rt, commands, resolve, reject]() {
                try {
#ifdef OP_SQLITE_USE_LIBSQL
                    auto batchResult =
                        opsqlite_libsql_execute_batch(db, &commands);
#else
                    auto batchResult = opsqlite_execute_batch(db, &commands);
#endif

                    if (invalidated) {
                        return;
                    }

                    invoker->invokeAsync([&rt,
                                          batchResult = std::move(batchResult),
                                          resolve] {
                        auto res = jsi::Object(rt);
                        res.setProperty(rt, "rowsAffected",
                                        jsi::Value(batchResult.affectedRows));
                        resolve->asObject(rt).asFunction(rt).call(
                            rt, std::move(res));
                    });
                } catch (std::runtime_error &e) {
                    auto what = e.what();
                    invoker->invokeAsync([&rt, what, reject] {
                        auto errorCtr =
                            rt.global().getPropertyAsFunction(rt, "Error");
                        auto error = errorCtr.callAsConstructor(
                            rt, jsi::String::createFromAscii(rt, what));
                        reject->asObject(rt).asFunction(rt).call(rt, error);
                    });
                } catch (std::exception &exc) {
                    auto what = exc.what();
                    invoker->invokeAsync([&rt, what, reject] {
                        auto errorCtr =
                            rt.global().getPropertyAsFunction(rt, "Error");
                        auto error = errorCtr.callAsConstructor(
                            rt, jsi::String::createFromAscii(rt, what));
                        reject->asObject(rt).asFunction(rt).call(rt, error);
                    });
                }
            };
            _thread_pool->queueWork(task);

            return {};
    }));

            return promise;
    });

#ifdef OP_SQLITE_USE_LIBSQL
    function_map["sync"] = HOSTFN("sync") {
        opsqlite_libsql_sync(db);
        return {};
    });
#else
    function_map["loadFile"] = HOSTFN("loadFile") {
        if (count < 1) {
            throw std::runtime_error(
                "[op-sqlite][loadFile] Incorrect parameter count");
            return {};
        }

        const std::string sqlFileName = args[0].asString(rt).utf8(rt);

        auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
    auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor") {
            auto resolve = std::make_shared<jsi::Value>(rt, args[0]);
            auto reject = std::make_shared<jsi::Value>(rt, args[1]);

            auto task = [&rt, this, sqlFileName, resolve, reject]() {
                try {
                    const auto result = import_sql_file(db, sqlFileName);

                    invoker->invokeAsync([&rt, result, resolve] {
                        auto res = jsi::Object(rt);
                        res.setProperty(rt, "rowsAffected",
                                        jsi::Value(result.affectedRows));
                        res.setProperty(rt, "commands",
                                        jsi::Value(result.commands));
                        resolve->asObject(rt).asFunction(rt).call(
                            rt, std::move(res));
                    });
                } catch (std::runtime_error &e) {
                    auto what = e.what();
                    invoker->invokeAsync(
                        [&rt, what = std::string(what), reject] {
                            auto errorCtr =
                                rt.global().getPropertyAsFunction(rt, "Error");
                            auto error = errorCtr.callAsConstructor(
                                rt, jsi::String::createFromAscii(rt, what));
                            reject->asObject(rt).asFunction(rt).call(rt, error);
                        });
                } catch (std::exception &exc) {
                    auto what = exc.what();
                    invoker->invokeAsync(
                        [&rt, what = std::string(what), reject] {
                            auto errorCtr =
                                rt.global().getPropertyAsFunction(rt, "Error");
                            auto error = errorCtr.callAsConstructor(
                                rt, jsi::String::createFromAscii(rt, what));
                            reject->asObject(rt).asFunction(rt).call(rt, error);
                        });
                }
            };
            _thread_pool->queueWork(task);
            return {};
    }));

    return promise;
    });

    function_map["updateHook"] = HOSTFN("updateHook") {
        auto callback = std::make_shared<jsi::Value>(rt, args[0]);

        if (callback->isUndefined() || callback->isNull()) {
            update_hook_callback = nullptr;
        } else {
            update_hook_callback = callback;
        }

        auto_register_update_hook();
        return {};
    });

    function_map["commitHook"] = HOSTFN("commitHook") {
        if (count < 1) {
            throw std::runtime_error("[op-sqlite][commitHook] callback needed");
        }

        auto callback = std::make_shared<jsi::Value>(rt, args[0]);
        if (callback->isUndefined() || callback->isNull()) {
            opsqlite_deregister_commit_hook(db);
            return {};
        }
        commit_hook_callback = callback;
        opsqlite_register_commit_hook(db, this);

        return {};
    });

    function_map["rollbackHook"] = HOSTFN("rollbackHook") {
        if (count < 1) {
            throw std::runtime_error(
                "[op-sqlite][rollbackHook] callback needed");
        }

        auto callback = std::make_shared<jsi::Value>(rt, args[0]);

        if (callback->isUndefined() || callback->isNull()) {
            opsqlite_deregister_rollback_hook(db);
            return {};
        }
        rollback_hook_callback = callback;

        opsqlite_register_rollback_hook(db, this);
        return {};
    });

    function_map["loadExtension"] = HOSTFN("loadExtension") {
        auto path = args[0].asString(rt).utf8(rt);
        std::string entry_point;
        if (count > 1 && args[1].isString()) {
            entry_point = args[1].asString(rt).utf8(rt);
        }

        opsqlite_load_extension(db, path, entry_point);
        return {};
    });

    function_map["reactiveExecute"] = HOSTFN("reactiveExecute") {
        auto query = args[0].asObject(rt);

        const std::string query_str =
            query.getProperty(rt, "query").asString(rt).utf8(rt);
        auto js_args = query.getProperty(rt, "arguments");
        auto js_discriminators =
            query.getProperty(rt, "fireOn").asObject(rt).asArray(rt);
        auto variant_args = to_variant_vec(rt, js_args);

        sqlite3_stmt *stmt = opsqlite_prepare_statement(db, query_str);
        opsqlite_bind_statement(stmt, &variant_args);

        auto callback =
            std::make_shared<jsi::Value>(query.getProperty(rt, "callback"));

        std::vector<TableRowDiscriminator> discriminators;

        for (size_t i = 0; i < js_discriminators.length(rt); i++) {
            auto js_discriminator =
                js_discriminators.getValueAtIndex(rt, i).asObject(rt);
            std::string table =
                js_discriminator.getProperty(rt, "table").asString(rt).utf8(rt);
            std::vector<int> ids;
            if (js_discriminator.hasProperty(rt, "ids")) {
                auto js_ids = js_discriminator.getProperty(rt, "ids")
                                  .asObject(rt)
                                  .asArray(rt);
                for (size_t j = 0; j < js_ids.length(rt); j++) {
                    ids.push_back(static_cast<int>(
                        js_ids.getValueAtIndex(rt, j).asNumber()));
                }
            }
            discriminators.push_back({table, ids});
        }

        std::shared_ptr<ReactiveQuery> reactiveQuery =
            std::make_shared<ReactiveQuery>(
                ReactiveQuery{stmt, discriminators, callback});

        reactive_queries.push_back(reactiveQuery);

        auto_register_update_hook();

        auto unsubscribe = HOSTFN("unsubscribe") {
            auto it = std::find(reactive_queries.begin(),
                                reactive_queries.end(), reactiveQuery);
            if (it != reactive_queries.end()) {
                reactive_queries.erase(it);
            }
            auto_register_update_hook();
            return {};
        });

        return unsubscribe;
    });
#endif

    function_map["prepareStatement"] = HOSTFN("prepareStatement") {
        auto query = args[0].asString(rt).utf8(rt);
#ifdef OP_SQLITE_USE_LIBSQL
        libsql_stmt_t statement = opsqlite_libsql_prepare_statement(db, query);
#else
        sqlite3_stmt *statement = opsqlite_prepare_statement(db, query);
#endif
        auto preparedStatementHostObject =
            std::make_shared<PreparedStatementHostObject>(
                db, db_name, statement, invoker, _thread_pool);

        return jsi::Object::createFromHostObject(rt,
                                                 preparedStatementHostObject);
    });

    function_map["getDbPath"] = HOSTFN("getDbPath") {
        std::string path = std::string(base_path);

        if (count == 1) {
            if (!args[0].isString()) {
                throw std::runtime_error(
                    "[op-sqlite][open] database location must be a string");
            }

            std::string last_path = args[0].asString(rt).utf8(rt);

            if (last_path == ":memory:") {
                path = ":memory:";
            } else if (last_path.rfind('/', 0) == 0) {
                path = last_path;
            } else {
                path = path + "/" + last_path;
            }
        }

        auto result = opsqlite_get_db_path(db_name, path);
        return jsi::String::createFromUtf8(rt, result);
    });

    function_map["flushPendingReactiveQueries"] =
        HOSTFN("flushPendingReactiveQueries") {
        auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
    auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor") {
            auto resolve = std::make_shared<jsi::Value>(rt, args[0]);

            auto task = [&rt, this, resolve]() {
                flush_pending_reactive_queries(resolve);
            };

            _thread_pool->queueWork(task);

            return {};
    }));

    return promise;
    });
}

std::vector<jsi::PropNameID> DBHostObject::getPropertyNames(jsi::Runtime &_rt) {
    std::vector<jsi::PropNameID> keys;
    keys.reserve(function_map.size());
    for (const auto &pair : function_map) {
        keys.emplace_back(jsi::PropNameID::forUtf8(_rt, pair.first));
    }
    return keys;
}

jsi::Value DBHostObject::get(jsi::Runtime &_rt,
                             const jsi::PropNameID &propNameID) {
    auto name = propNameID.utf8(rt);
    if (function_map.count(name) != 1) {
        return HOST_STATIC_FN(name.c_str()) {
            throw std::runtime_error(
                "[op-sqlite] Function " + name +
                " not implemented for current backend (libsql or sqlcipher)");
        });
    }

    return {rt, function_map[name]};
}

void DBHostObject::set(jsi::Runtime &_rt, const jsi::PropNameID &name,
                       const jsi::Value &value) {
    throw std::runtime_error("You cannot write to this object!");
}

void DBHostObject::invalidate() {
    if (invalidated) {
        return;
    }

    invalidated = true;
    _thread_pool->restartPool();
#ifdef OP_SQLITE_USE_LIBSQL
    opsqlite_libsql_close(db);
#else
    if (db != nullptr) {
        opsqlite_close(db);
        db = nullptr;
    }
#endif
}

DBHostObject::~DBHostObject() { invalidate(); }

} // namespace opsqlite
