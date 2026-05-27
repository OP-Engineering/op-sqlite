#pragma once

#include <jsi/jsi.h>
#include <ReactCommon/CallInvoker.h>
#ifdef __ANDROID__
#include "sqlite3.h"
#else
#include <sqlite3.h>
#endif
#include "OPThreadPool.h"

namespace opsqlite {

namespace jsi = facebook::jsi;

class OPDB: public jsi::NativeState {
public:
  OPDB(const std::string &name, const std::string &path, const std::string &sqlite_vec_path);
  std::shared_ptr<ThreadPool> thread_pool;
  sqlite3 *db;
  std::string name;
  std::string path;
};
}


