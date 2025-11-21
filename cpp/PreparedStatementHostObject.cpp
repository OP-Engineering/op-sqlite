#include "PreparedStatementHostObject.h"
#if OP_SQLITE_USE_LIBSQL
#include "libsql/bridge.h"
#else
#include "bridge.h"
#endif
#include "macros.h"
#include "utils.hpp"

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
    return HFN(this) {
      if (_stmt == nullptr) {
        throw std::runtime_error("statement has been freed");
      }

      const jsi::Value &js_params = args[0];
      std::vector<JSVariant> params = to_variant_vec(rt, js_params);

      return promisify(
          rt,
          [this, params]() {
#ifdef OP_SQLITE_USE_LIBSQL
            opsqlite_libsql_bind_statement(_stmt, &params);
#else
            opsqlite_bind_statement(_stmt, &params);
#endif
            return nullptr;
          },
          [](jsi::Runtime &rt, std::any result) {
            return jsi::Value::undefined;
          });
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

      return promisify(
          rt,
          [this]() {
            std::vector<DumbHostObject> results;
            auto metadata = std::make_shared<std::vector<SmartHostObject>>();
#ifdef OP_SQLITE_USE_LIBSQL
            auto status = opsqlite_libsql_execute_prepared_statement(
                _db, _stmt, &results, metadata);
#else
            auto status = opsqlite_execute_prepared_statement(
                _db, _stmt, &results, metadata);
#endif
            return std::make_tuple(results, *metadata, status);
          },
          [](jsi::Runtime &rt, std::any result) {
            auto tuple = std::any_cast<
                std::tuple<std::vector<DumbHostObject>,
                           std::vector<SmartHostObject>, BridgeResult>>(result);
            const auto &results = std::get<0>(tuple);
            const auto &metadata = std::get<1>(tuple);
            BridgeResult status = std::get<2>(tuple);
            auto results_ptr =
                std::make_shared<std::vector<DumbHostObject>>(results);
            auto metadata_ptr =
                std::make_shared<std::vector<SmartHostObject>>(metadata);
            return create_result(rt, status, results_ptr.get(), metadata_ptr);
          });
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
