#include "PreparedStatementHostObject.h"
#if OP_SQLITE_USE_LIBSQL
#include "libsql/bridge.h"
#else
#include "bridge.h"
#endif
#include "macros.h"
#include "utils.h"

namespace opsqlite {

namespace jsi = facebook::jsi;

std::vector<jsi::PropNameID>
PreparedStatementHostObject::getPropertyNames(jsi::Runtime &rt) {
    std::vector<jsi::PropNameID> keys;

    return keys;
}

jsi::Value PreparedStatementHostObject::get(jsi::Runtime &rt,
                                            const jsi::PropNameID &propNameID) {
    auto name = propNameID.utf8(rt);

    if (name == "bind") {
        return HOSTFN("bind") {
            if (_stmt == nullptr) {
                throw std::runtime_error("statement has been freed");
            }

            const jsi::Value &js_params = args[0];
            std::vector<JSVariant> params = to_variant_vec(rt, js_params);

            auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
      auto promise = promiseCtr.callAsConstructor(
          rt, HOSTFN("executor") {
                auto resolve = std::make_shared<jsi::Value>(rt, args[0]);
                auto reject = std::make_shared<jsi::Value>(rt, args[1]);
                auto task = [&rt, this, resolve, reject,
                             invoker = this->_js_call_invoker, params]() {
                    try {
#ifdef OP_SQLITE_USE_LIBSQL
                        opsqlite_libsql_bind_statement(_stmt, &params);
#else
                        opsqlite_bind_statement(_stmt, &params);
#endif
                        invoker->invokeAsync([&rt, resolve] {
                            resolve->asObject(rt).asFunction(rt).call(rt, {});
                        });
                    } catch (const std::runtime_error &e) {
                        invoker->invokeAsync([&rt, e, reject] {
                            auto errorCtr =
                                rt.global().getPropertyAsFunction(rt, "Error");
                            auto error = errorCtr.callAsConstructor(
                                rt, jsi::String::createFromUtf8(rt, e.what()));
                            reject->asObject(rt).asFunction(rt).call(rt, error);
                        });
                    } catch (const std::exception &e) {
                        invoker->invokeAsync([&rt, e, reject] {
                            auto errorCtr =
                                rt.global().getPropertyAsFunction(rt, "Error");
                            auto error = errorCtr.callAsConstructor(
                                rt, jsi::String::createFromUtf8(rt, e.what()));
                            reject->asObject(rt).asFunction(rt).call(rt, error);
                        });
                    }
                };

                _thread_pool->queueWork(task);

                return {};
          }));
      return promise;
        });
    }

    if (name == "bindSync") {
      return HOSTFN("bindSync") {
        if (_stmt == nullptr) {
          throw std::runtime_error("statement has been freed");
        }
        
        const jsi::Value &js_params = args[0];
        std::vector<JSVariant> params = to_variant_vec(rt, js_params);
        try {
#ifdef OP_SQLITE_USE_LIBSQL
          opsqlite_libsql_bind_statement(_stmt, &params);
#else
          opsqlite_bind_statement(_stmt, &params);
#endif
        } catch (const std::runtime_error &e) {
          throw std::runtime_error(e.what());
        } catch (const std::exception &e) {
          throw std::runtime_error(e.what());
        }
        return {};
      });
    }

    if (name == "execute") {
        return HOSTFN("execute") {
            if (_stmt == nullptr) {
                throw std::runtime_error("statement has been freed");
            }

            auto promiseCtr = rt.global().getPropertyAsFunction(rt, "Promise");
        auto promise = promiseCtr.callAsConstructor(rt, HOSTFN("executor") {
                auto resolve = std::make_shared<jsi::Value>(rt, args[0]);
                auto reject = std::make_shared<jsi::Value>(rt, args[1]);

                auto task = [&rt, this, resolve, reject,
                             invoker = this->_js_call_invoker]() {
                    std::vector<DumbHostObject> results;
                    std::shared_ptr<std::vector<SmartHostObject>> metadata =
                        std::make_shared<std::vector<SmartHostObject>>();
                    try {
#ifdef OP_SQLITE_USE_LIBSQL
                        auto status =
                            opsqlite_libsql_execute_prepared_statement(
                                _db, _stmt, &results, metadata);
#else
                        auto status = opsqlite_execute_prepared_statement(
                            _db, _stmt, &results, metadata);
#endif
                        invoker->invokeAsync(
                            [&rt, status = std::move(status),
                             results =
                                 std::make_shared<std::vector<DumbHostObject>>(
                                     results),
                             metadata, resolve] {
                                auto jsiResult = create_result(
                                    rt, status, results.get(), metadata);
                                resolve->asObject(rt).asFunction(rt).call(
                                    rt, std::move(jsiResult));
                            });
                    } catch (std::exception &exc) {
                        invoker->invokeAsync([&rt, &exc, reject] {
                            auto errorCtr =
                                rt.global().getPropertyAsFunction(rt, "Error");
                            auto error = errorCtr.callAsConstructor(
                                rt,
                                jsi::String::createFromUtf8(rt, exc.what()));
                            reject->asObject(rt).asFunction(rt).call(rt, error);
                        });
                    }
                };

                _thread_pool->queueWork(task);

                return {};
          }));

        return promise;
        });
    }

    return {};
}

PreparedStatementHostObject::~PreparedStatementHostObject() {
#ifdef OP_SQLITE_USE_LIBSQL
    if (_stmt != nullptr) {
        libsql_free_stmt(_stmt);
        _stmt = nullptr;
    }
#else
    if (_stmt != nullptr) {
        //    sqlite3_finalize(_stmt);
        _stmt = nullptr;
    }
#endif
}

} // namespace opsqlite
