#include "DB.hpp"
// #include "PreparedStatementHostObject.h"
// #if OP_SQLITE_USE_LIBSQL
// #include "libsql/bridge.h"
// #else
#include "bridge.h"
// #endif
#include "logs.h"
#include "macros.h"
#include "utils.hpp"
#include <iostream>
#include <jsi/jsi.h>
#include <utility>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

jsi::Object create_db(jsi::Runtime &rt,
                      std::shared_ptr<react::CallInvoker> invoker,
                      const std::string &path) {
  auto res = jsi::Object(rt);
  auto db = opsqlite_open_v2(path);

  auto native = std::make_shared<NativeDB>(path, db, invoker);
  res.setNativeState(rt, native);

  auto executeSync = host_fn(rt, [](jsi::Runtime &rt, const jsi::Value &thiz,
                                    const jsi::Value *args, size_t count) {
    auto native = thiz.asObject(rt).getNativeState<NativeDB>(rt);
    std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params;

    if (count == 2) {
      params = to_variant_vec(rt, args[1]);
    }

#ifdef OP_SQLITE_USE_LIBSQL
    auto status = opsqlite_libsql_execute(db, query, &params);
#else
    auto status = opsqlite_execute(native->db, query, &params);
#endif

    return create_js_rows(rt, status);
  });
  res.setProperty(rt, "executeSync", executeSync);

  auto execute = host_fn(rt, [](jsi::Runtime &rt, const jsi::Value &thiz,
                                const jsi::Value *args, size_t count) {
    const std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                        ? to_variant_vec(rt, args[1])
                                        : std::vector<JSVariant>();
    auto native = thiz.asObject(rt).getNativeState<NativeDB>(rt);

    return promisify(
        rt, native->invoker,
        [&rt, native, query, params](std::shared_ptr<jsi::Value> resolve) {
#ifdef OP_SQLITE_USE_LIBSQL
          auto status = opsqlite_libsql_execute(db, query, &params);
#else
          auto status = opsqlite_execute(native->db, query, &params);
#endif

          //        if (invalidated) {
          //          return;
          //        }

          native->invoker->invokeAsync([&rt, status = std::move(status),
                                        resolve] {
            auto jsiResult = create_js_rows(rt, status);
            resolve->asObject(rt).asFunction(rt).call(rt, std::move(jsiResult));
          });
        });
  });
  res.setProperty(rt, "execute", execute);

  return res;
}
} // namespace opsqlite
