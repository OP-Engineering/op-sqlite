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
