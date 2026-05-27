#include "OPDB.hpp"
#include "OPBridge.hpp"

namespace opsqlite {
namespace jsi = facebook::jsi;
namespace react = facebook::react;

OPDB::OPDB(const std::string &name, const std::string &path, const std::string &sqlite_vec_path): name(name), path(path) {
  thread_pool = std::make_shared<ThreadPool>();
  
#ifdef OP_SQLITE_USE_SQLCIPHER
  db = opsqlite_open(db_name, path, crsqlite_path, sqlite_vec_path,
                     encryption_key);
#elif OP_SQLITE_USE_LIBSQL
  db = opsqlite_libsql_open(db_name, path, crsqlite_path);
#else
  db = opsqlite_open(name, path, sqlite_vec_path);
#endif
  
}
}
