//
//  PreparedStatementHostObject.hpp
//  op-sqlite
//
//  Created by Oscar Franco on 5/12/23.
//

#ifndef PreparedStatementHostObject_h
#define PreparedStatementHostObject_h

#include <jsi/jsi.h>
#include <memory>
#include <sqlite3.h>
#include <string>

namespace opsqlite {
namespace jsi = facebook::jsi;

class PreparedStatementHostObject : public jsi::HostObject {
public:
  PreparedStatementHostObject(std::string dbName, sqlite3_stmt *statement);
  virtual ~PreparedStatementHostObject();

  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt);

  jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propNameID);

private:
  std::string _dbName;
  // This shouldn't be de-allocated until sqlite3_finalize is called on it
  sqlite3_stmt *_statement;
};

} // namespace opsqlite

#endif /* PreparedStatementHostObject_hpp */
