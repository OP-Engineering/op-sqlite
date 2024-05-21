#ifndef DBHostObject_h
#define DBHostObject_h

#include "ThreadPool.h"
#include "types.h"
#include <ReactCommon/CallInvoker.h>
#include <any>
#include <jsi/jsi.h>
#include <unordered_map>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

class JSI_EXPORT DBHostObject : public jsi::HostObject {
public:
  DBHostObject(jsi::Runtime &rt, std::string &base_path,
               std::shared_ptr<react::CallInvoker> js_call_invoker,
               std::shared_ptr<ThreadPool> thread_pool, std::string &db_name,
               std::string &path, std::string &crsqlite_path,
               std::string &encryption_key);

  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt);

  jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propNameID);

  std::unordered_map<std::string, jsi::Value> function_map;
  std::string base_path;

  std::shared_ptr<jsi::Value> update_hook;
  std::shared_ptr<react::CallInvoker> jsCallInvoker;
  std::shared_ptr<ThreadPool> thread_pool;
  std::string db_name;
};

} // namespace opsqlite

#endif /* DBHostObject_h */
