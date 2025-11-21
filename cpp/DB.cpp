#include "DB.hpp"
// #include "PreparedStatementHostObject.h"
// #if OP_SQLITE_USE_LIBSQL
// #include "libsql/bridge.h"
// #else
#include "bridge.h"
// #endif
#include "logs.h"
#include "macros.h"
#include "types.hpp"
#include "utils.hpp"
#include <iostream>
#include <jsi/jsi.h>
#include <utility>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

jsi::Object create_db(jsi::Runtime &rt, const std::string &path) {
  auto res = jsi::Object(rt);
  sqlite3 *db = opsqlite_open_v2(path);

  // EXECUTE SYNC
  auto executeSync = HFN(db) {
    const std::string query = args[0].asString(rt).utf8(rt);
    const std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                              ? to_variant_vec(rt, args[1])
                                              : std::vector<JSVariant>();

#ifdef OP_SQLITE_USE_LIBSQL
    auto status = opsqlite_libsql_execute(db, query, &params);
#else
    auto status = opsqlite_execute(db, query, &params);
#endif

    return create_js_rows_2(rt, status);
  });
  res.setProperty(rt, "executeSync", executeSync);

  // EXECUTE
  auto execute = HFN(db) {
    const std::string query = args[0].asString(rt).utf8(rt);
    const std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                              ? to_variant_vec(rt, args[1])
                                              : std::vector<JSVariant>();

    return promisify(
        rt,
        [db, query, params]() {
#ifdef OP_SQLITE_USE_LIBSQL
          return opsqlite_libsql_execute(db, query, &params);
#else
          return opsqlite_execute(db, query, &params);
#endif
        },
        [](jsi::Runtime &rt, std::any results) {
          auto status = std::any_cast<BridgeResult>(results);
          return create_js_rows_2(rt, status);
        });
  });
  res.setProperty(rt, "execute", execute);

  // CLOSE
  auto close = HFN(db) {
#ifdef OP_SQLITE_USE_LIBSQL
    opsqlite_libsql_close(db);
#else
    opsqlite_close(db);
#endif

    return {};
  });
  res.setProperty(rt, "close", close);

  return res;
}
} // namespace opsqlite
