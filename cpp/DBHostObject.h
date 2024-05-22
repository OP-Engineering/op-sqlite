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

struct ReactiveQuery {
  std::string query;
  std::vector<JSVariant> args;
  std::vector<std::string> tables;
  std::vector<int> rowIds;
  std::shared_ptr<jsi::Value> callback;
};

class JSI_EXPORT DBHostObject : public jsi::HostObject {
public:
  DBHostObject(jsi::Runtime &rt, std::string &base_path,
               std::shared_ptr<react::CallInvoker> js_call_invoker,
               std::shared_ptr<ThreadPool> thread_pool, std::string &db_name,
               std::string &path, std::string &crsqlite_path,
               std::string &encryption_key);

  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt);

  jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propNameID);

  void auto_register_update_hook();

  std::unordered_map<std::string, jsi::Value> function_map;
  std::string base_path;

  std::shared_ptr<jsi::Value> update_hook;
  std::shared_ptr<react::CallInvoker> jsCallInvoker;
  std::shared_ptr<ThreadPool> thread_pool;
  std::string db_name;
  std::shared_ptr<jsi::Value> update_hook_callback;
  jsi::Runtime &rt;
  std::vector<ReactiveQuery> reactive_queries;
  bool has_update_hook_registered = false;
};

} // namespace opsqlite

#endif /* DBHostObject_h */
