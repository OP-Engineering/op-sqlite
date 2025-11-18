#pragma once

#define HOSTFN(name)                   \
jsi::Function::createFromHostFunction( \
rt, \
jsi::PropNameID::forAscii(rt, name), \
0, \
[=, this](jsi::Runtime &rt, const jsi::Value &thisValue, const jsi::Value *args, size_t count) -> jsi::Value

#define HOST_STATIC_FN(name)           \
jsi::Function::createFromHostFunction( \
rt, \
jsi::PropNameID::forAscii(rt, name), \
0, \
[=](jsi::Runtime &rt, const jsi::Value &thisValue, const jsi::Value *args, size_t count) -> jsi::Value

#define HFN(captures)                            \
jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forAscii(rt, ""), 0, [captures](jsi::Runtime &rt, const jsi::Value &that, const jsi::Value *args, size_t count) -> jsi::Value


#define HFN2(captures, captures2)                            \
jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forAscii(rt, ""), 0, [captures, captures2](jsi::Runtime &rt, const jsi::Value &that, const jsi::Value *args, size_t count) -> jsi::Value
