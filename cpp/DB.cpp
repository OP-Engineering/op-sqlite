#include "DB.hpp"
// #include "PreparedStatementHostObject.h"
// #if OP_SQLITE_USE_LIBSQL
// #include "libsql/bridge.h"
// #else
#include "bridge.h"
// #endif
#include "logs.h"
#include "macros.h"
#include "types.h"
#include "utils.hpp"
#include <iostream>
#include <jsi/jsi.h>
#include <utility>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

jsi::Object create_db(jsi::Runtime &rt,
                      const std::shared_ptr<react::CallInvoker> &invoker,
                      const std::string &path) {
  auto res = jsi::Object(rt);
  auto state = std::make_shared<State>();
  state->db = opsqlite_open_v2(path);
  state->invoker = invoker;
  // auto db = std::shared_ptr<sqlite3>(opsqlite_open_v2(path), opsqlite_close);
  auto invalidated = std::make_shared<bool>(false);

  auto executeSync = HFN(state) {
    const std::string query = args[0].asString(rt).utf8(rt);
    const std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                              ? to_variant_vec(rt, args[1])
                                              : std::vector<JSVariant>();

#ifdef OP_SQLITE_USE_LIBSQL
    auto status = opsqlite_libsql_execute(state->db, query, &params);
#else
    auto status = opsqlite_execute(state->db, query, &params);
#endif

    return create_js_rows_2(rt, status);
  });
  res.setProperty(rt, "executeSync", executeSync);

  auto execute = HFN2(state, invoker) {
    const std::string query = args[0].asString(rt).utf8(rt);
    const std::vector<JSVariant> params = count == 2 && args[1].isObject()
                                              ? to_variant_vec(rt, args[1])
                                              : std::vector<JSVariant>();

    return promisify(
        rt, state,
        [state, query, params]() {
#ifdef OP_SQLITE_USE_LIBSQL
          return opsqlite_libsql_execute(state->db, query, &params);
#else
          return opsqlite_execute(state->db, query, &params);
#endif
        },
        [](jsi::Runtime &rt, std::any results) {
          auto status = std::any_cast<BridgeResult>(results);
          return create_js_rows_2(rt, status);
        });
  });
  res.setProperty(rt, "execute", execute);

  auto close = HFN(state) {
    state->invalidated = true;

#ifdef OP_SQLITE_USE_LIBSQL
    opsqlite_libsql_close(db);
#else
    opsqlite_close(state->db);
#endif

    return {};
  });

  return res;
}
} // namespace opsqlite
