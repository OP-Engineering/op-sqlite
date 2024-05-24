#pragma once

#include <jsi/jsi.h>
#include <memory>
#ifdef OP_SQLITE_USE_LIBSQL
#include "libsql.h"
#else
#include <sqlite3.h>
#endif
#include <string>

namespace opsqlite {
namespace jsi = facebook::jsi;

class PreparedStatementHostObject : public jsi::HostObject {
public:
#ifdef OP_SQLITE_USE_LIBSQL
    PreparedStatementHostObject(std::string name, libsql_stmt_t stmt): _name(name), _stmt(stmt) {};
#else
    PreparedStatementHostObject(std::string name, sqlite3_stmt *stmt): _name(name), _stmt(stmt) {};
#endif
  virtual ~PreparedStatementHostObject();

  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt);

  jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propNameID);

private:
  std::string _name;
#ifdef OP_SQLITE_USE_LIBSQL
  libsql_stmt_t _stmt;
#else
  // This shouldn't be de-allocated until sqlite3_finalize is called on it
  sqlite3_stmt *_stmt;
#endif
};

} // namespace opsqlite
