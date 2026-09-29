#pragma once

#include "OPDumbHostObject.hpp"
#include "OPSmartHostObject.hpp"
#include "OPTypes.hpp"
#include <jsi/jsi.h>
#ifdef __ANDROID__
#include "sqlite3.h"
#else
#include <sqlite3.h>
#endif
#include <ReactCommon/CallInvoker.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "OPThreadPool.hpp"

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

jsi::Value to_jsi(jsi::Runtime &rt, const JSVariant &value);

JSVariant to_variant(jsi::Runtime &rt, jsi::Value const &value);

std::vector<std::string> to_string_vec(jsi::Runtime &rt, jsi::Value const &xs);

std::vector<JSVariant> to_variant_vec(jsi::Runtime &rt, jsi::Value const &xs);

std::vector<int> to_int_vec(jsi::Runtime &rt, jsi::Value const &xs);

jsi::Value
create_result(jsi::Runtime &rt, const BridgeResult &status,
              std::vector<DumbHostObject> *results,
              std::shared_ptr<std::vector<SmartHostObject>> metadata);

jsi::Value create_js_rows(jsi::Runtime &rt, const BridgeResult &status);

jsi::Value
create_raw_result(jsi::Runtime &rt, const BridgeResult &status,
                  const std::vector<std::vector<JSVariant>> *results);

void to_batch_arguments(jsi::Runtime &rt, jsi::Array const &batch_params,
                        std::vector<BatchArguments> *commands);

BatchResult import_sql_file(sqlite3 *db, std::string path);

/// Runs `work` between `begin` and COMMIT, rolling back if either throws.
///
/// SQLite sometimes rolls the transaction back on its own before we get to it
/// (RAISE(ROLLBACK) in a trigger, a COMMIT failing with SQLITE_FULL or
/// SQLITE_IOERR*). A ROLLBACK then fails with "cannot rollback - no
/// transaction is active", which must never replace the original exception.
///
/// `in_transaction` returns std::nullopt on backends that cannot report the
/// transaction state (libsql): the ROLLBACK is always attempted there and its
/// failure dropped.
template <typename Execute, typename InTransaction, typename Work>
auto run_in_transaction(Execute &&execute, InTransaction &&in_transaction,
                        const char *begin, Work &&work) -> decltype(work()) {
  execute(begin);
  try {
    auto result = work();
    execute("COMMIT");
    return result;
  } catch (std::exception &error) {
    std::optional<bool> open = in_transaction();
    if (!open.has_value() || *open) {
      try {
        execute("ROLLBACK");
      } catch (std::exception &rollback_error) {
        // Every following statement would silently run inside the failed
        // transaction, which matters more than the original error
        std::optional<bool> still_open = in_transaction();
        if (still_open.has_value() && *still_open) {
          throw std::runtime_error(
              std::string("[op-sqlite] ROLLBACK failed and the connection is "
                          "still inside a transaction: ") +
              rollback_error.what() + ". Original error: " + error.what());
        }
      }
    }
    // Rethrow the original exception object: `throw error` would copy it
    // into a plain std::exception, dropping the SQLite result codes.
    throw;
  }
}

bool folder_exists(const std::string &name);

bool file_exists(const std::string &path);

void log_to_console(jsi::Runtime &rt, const std::string &message);

/// Creates a JS `Error` with the SQLite result codes attached as `code` and
/// `extendedCode`. Pass a negative code to leave both properties out, which is
/// what non-SQLite failures (bad arguments, closed database, ...) do.
jsi::Value create_js_error(jsi::Runtime &rt, const std::string &message,
                           int code, int extended_code);

/// Rethrows a SQLite failure so the result codes survive the trip into JS.
///
/// Needed on the synchronous paths only: JSI turns a C++ exception escaping a
/// host function into a JS Error built from what() alone, dropping any
/// properties. A jsi::JSError carries our own Error object through untouched.
[[noreturn]] void throw_js_error(jsi::Runtime &rt, const SQLiteError &error);

jsi::Value
promisify(jsi::Runtime &rt, std::shared_ptr<ThreadPool> thread_pool, std::function<std::any()> lambda,
          std::function<jsi::Value(jsi::Runtime &rt, std::any result)>
              resolve_callback);

} // namespace opsqlite
