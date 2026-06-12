#pragma once

#include "DumbHostObject.h"
#include "SmartHostObject.h"
#include "types.hpp"
#include <jsi/jsi.h>
#ifdef __ANDROID__
#include "sqlite3.h"
#else
#include <sqlite3.h>
#endif
#include <ReactCommon/CallInvoker.h>
#include <string>
#include <vector>
#include "OPThreadPool.h"

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

struct JSIStringDataAppender {
    std::string *out;

    void operator()(bool ascii, const void *data, size_t num) const {
        if (ascii) {
            out->append(static_cast<const char *>(data), num);
            return;
        }

        const auto *u16 = static_cast<const char16_t *>(data);
        out->reserve(out->size() + num);
        for (size_t i = 0; i < num; i++) {
            out->push_back(static_cast<char>(u16[i]));
        }
    }
};

inline std::string jsi_string_to_utf8(jsi::Runtime &rt,
                                      const jsi::String &value) {
    std::string result;
    value.getStringData(rt, JSIStringDataAppender{&result});
    return result;
}

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

jsi::Value
promisify(jsi::Runtime &rt, std::shared_ptr<ThreadPool> thread_pool, std::function<std::any()> lambda,
          std::function<jsi::Value(jsi::Runtime &rt, std::any result)>
              resolve_callback);

} // namespace opsqlite
