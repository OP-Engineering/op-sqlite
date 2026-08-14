#include "OPSqlite.hpp"
#include "logs.h"
#include <ReactCommon/CallInvokerHolder.h>
#include <atomic>
#include <fbjni/fbjni.h>
#include <jni.h>
#include <jsi/jsi.h>
#include <memory>
#include <typeinfo>

namespace jsi = facebook::jsi;
namespace react = facebook::react;
namespace jni = facebook::jni;

// This file is not using raw jni but rather fbjni, do not change how the native
// functions are registered
// https://github.com/facebookincubator/fbjni/blob/main/docs/quickref.md
struct OPSQLiteBridge : jni::JavaClass<OPSQLiteBridge> {
  static constexpr auto kJavaDescriptor = "Lcom/op/sqlite/OPSQLiteBridge;";

  static void registerNatives() {
    javaClassStatic()->registerNatives(
        {makeNativeMethod("installNativeJsi", OPSQLiteBridge::installNativeJsi),
         makeNativeMethod("clearStateNativeJsi",
                          OPSQLiteBridge::clearStateNativeJsi)});
  }

private:
  // OPSQLiteBridge is a Kotlin singleton shared by every generation (see
  // OPSQLiteBridge.kt), so it can't carry per-generation identity itself.
  // installNativeJsi() heap-allocates a shared_ptr and hands the caller back
  // a raw handle to it; OPSQLiteModule (which IS recreated per generation)
  // holds that handle and passes it back to clearStateNativeJsi(), which is
  // what lets invalidate() flip THIS generation's liveness flag rather than
  // a process-global a newer, overlapping generation may have already
  // reassigned. Released by clearStateNativeJsi().
  static jlong installNativeJsi(
      jni::alias_ref<jni::JObject> thiz, jlong jsiRuntimePtr,
      jni::alias_ref<react::CallInvokerHolder::javaobject> jsCallInvokerHolder,
      jni::alias_ref<jni::JString> dbPath) {
    auto jsiRuntime = reinterpret_cast<jsi::Runtime *>(jsiRuntimePtr);
    auto jsCallInvoker = jsCallInvokerHolder->cthis()->getCallInvoker();
    std::string dbPathStr = dbPath->toStdString();

    auto generation_alive = opsqlite::install(
        *jsiRuntime, jsCallInvoker, dbPathStr.c_str(), "libcrsqlite",
        "libsqlite_vec");

    auto *handle = new std::shared_ptr<std::atomic<bool>>(generation_alive);
    return reinterpret_cast<jlong>(handle);
  }

  static void clearStateNativeJsi(jni::alias_ref<jni::JObject> thiz,
                                  jlong handlePtr) {
    auto *handle =
        reinterpret_cast<std::shared_ptr<std::atomic<bool>> *>(handlePtr);
    if (handle == nullptr) {
      return;
    }

    opsqlite::invalidate(*handle);
    delete handle;
  }
};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *) {
  return jni::initialize(vm, [] { OPSQLiteBridge::registerNatives(); });
}