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
// namespace react = facebook::react;

jsi::Object create_db(jsi::Runtime &rt, const std::string &path) {
    auto res = jsi::Object(rt);
    auto db = opsqlite_open_v2(path);

    auto native_db = std::make_shared<NativeDB>(path, db);
    res.setNativeState(rt, native_db);

  auto executeSync = host_fn(rt, [](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count){
    auto native_db =
    thisVal.asObject(rt).getNativeState<NativeDB>(rt);
    std::string query = args[0].asString(rt).utf8(rt);
    std::vector<JSVariant> params;
    
    if (count == 2) {
      params = to_variant_vec(rt, args[1]);
    }
    
#ifdef OP_SQLITE_USE_LIBSQL
    auto status =
    opsqlite_libsql_execute(db, query, &params);
#else
    auto status =
    opsqlite_execute(native_db->db, query, &params);
#endif
    
    return create_js_rows(rt, status);
  });
  res.setProperty(rt, "executeSync", executeSync);

    return res;
}
} // namespace opsqlite
