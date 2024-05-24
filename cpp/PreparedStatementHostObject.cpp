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
                                             const jsi::PropNameID
                                             &propNameID) {
  auto name = propNameID.utf8(rt);

  if (name == "bind") {
    return HOSTFN("bind", 1) {
      if (_stmt == nullptr) {
        throw std::runtime_error("statement has been freed");
      }

      const jsi::Value &js_params = args[0];
      std::vector<JSVariant> params = to_variant_vec(rt, js_params);
#ifdef OP_SQLITE_USE_LIBSQL
        opsqlite_libsql_bind_statement(_stmt, &params);
#else
        opsqlite_bind_statement(_stmt, &params);
#endif

      return {};
    });
  }

  if (name == "execute") {
    return HOSTFN("execute", 1) {
      if (_stmt == nullptr) {
        throw std::runtime_error("statement has been freed");
      }

      std::vector<DumbHostObject> results;
      std::shared_ptr<std::vector<SmartHostObject>> metadata =
          std::make_shared<std::vector<SmartHostObject>>();
#ifdef OP_SQLITE_USE_LIBSQL
        auto status = opsqlite_libsql_execute_prepared_statement(_name, _stmt,
                                                          &results, metadata);
#else
      auto status = opsqlite_execute_prepared_statement(_name, _stmt,
                                                        &results, metadata);
#endif

      if (status.type == SQLiteError) {
        throw std::runtime_error(status.message);
      }

      auto jsiResult = createResult(rt, status, &results, metadata);
      return jsiResult;
    });
  }

  return {};
}

 PreparedStatementHostObject::~PreparedStatementHostObject() {
#ifdef OP_SQLITE_USE_LIBSQL
     if(_stmt != nullptr) {
         libsql_free_stmt(_stmt);
         _stmt = nullptr;
     }
#else
     if (_stmt != nullptr) {
         sqlite3_finalize(_stmt);
         _stmt = nullptr;
     }
#endif
}

} // namespace opsqlite
