#include "bindings.h"
#include "logs.h"
#include <ReactCommon/CallInvokerHolder.h>
#include <fbjni/fbjni.h>
#include <jni.h>
#include <jsi/jsi.h>
#include <typeinfo>

namespace jni = facebook::jni;
namespace react = facebook::react;
namespace jsi = facebook::jsi;

std::string jstring2string(JNIEnv *env, jstring jStr) {
    if (!jStr)
        return "";

    const jclass stringClass = env->GetObjectClass(jStr);
    const jmethodID getBytes = env->GetMethodID(stringClass, "getBytes", "(Ljava/lang/String;)[B");
    const jbyteArray stringJbytes = (jbyteArray) env->CallObjectMethod(jStr, getBytes, env->NewStringUTF("UTF-8"));

    size_t length = (size_t) env->GetArrayLength(stringJbytes);
    jbyte* pBytes = env->GetByteArrayElements(stringJbytes, NULL);

    std::string ret = std::string((char *)pBytes, length);
    env->ReleaseByteArrayElements(stringJbytes, pBytes, JNI_ABORT);

    env->DeleteLocalRef(stringJbytes);
    env->DeleteLocalRef(stringClass);
    return ret;
}

extern "C" JNIEXPORT void JNICALL
Java_com_op_sqlite_OPSQLiteBridge_clearStateNativeJsi(JNIEnv *env,
                                                      jobject clazz) {
  opsqlite::clearState();
}

extern "C" JNIEXPORT void JNICALL
Java_com_op_sqlite_OPSQLiteBridge_installNativeJsi(JNIEnv *env, jobject clazz,
                                                   jlong jsiRuntimePtr,
                                                   jobject jsCallInvokerHolder,
                                                   jstring dbPath) {
  auto jsiRuntime = reinterpret_cast<jsi::Runtime *>(jsiRuntimePtr);
  auto callInvoker = reinterpret_cast<react::CallInvokerHolder *>(jsCallInvokerHolder)->getCallInvoker();
    std::string dbPathStr = jstring2string(env, dbPath);

  opsqlite::install(*jsiRuntime, callInvoker, dbPathStr.c_str());
}
