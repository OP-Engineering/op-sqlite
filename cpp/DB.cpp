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
  auto db = std::shared_ptr<sqlite3>(opsqlite_open_v2(path), opsqlite_close);

  auto executeSync = HFN(db) {
    std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params;

    if (count == 2) {
      params = to_variant_vec(rt, args[1]);
    }

#ifdef OP_SQLITE_USE_LIBSQL
    auto status = opsqlite_libsql_execute(db, query, &params);
#else
    auto status = opsqlite_execute(db.get(), query, &params);
#endif

    return create_js_rows(rt, status);
  });
  res.setProperty(rt, "executeSync", executeSync);

  auto execute =
      HFN2(db, invoker) {
        const std::string query = args[0].asString(rt).utf8(rt);
        const std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                                  ? to_variant_vec(rt, args[1])
                                                  : std::vector<JSVariant>();

        return promisify(
            rt, invoker,
            [&db, query = std::move(query), params = std::move(params)]() {
#ifdef OP_SQLITE_USE_LIBSQL
              return opsqlite_libsql_execute(db.get(), query, &params);
#else
              return opsqlite_execute(db.get(), query, &params);
#endif
            },
            [](jsi::Runtime &rt, std::any results) {
              auto status = std::any_cast<BridgeResult>(results);
              return create_js_rows(rt, status);
            });
      });
  res.setProperty(rt, "execute", execute);

  return res;
}
} // namespace opsqlite
