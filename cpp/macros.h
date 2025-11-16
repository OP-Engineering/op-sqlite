#pragma once

#define HOSTFN(name)                                                           \
jsi::Function::createFromHostFunction( \
rt, \
jsi::PropNameID::forAscii(rt, name), \
0, \
[=, this](jsi::Runtime &rt, const jsi::Value &thisValue, const jsi::Value *args, size_t count) -> jsi::Value

#define HOST_STATIC_FN(name)                                                   \
jsi::Function::createFromHostFunction( \
rt, \
jsi::PropNameID::forAscii(rt, name), \
0, \
[=](jsi::Runtime &rt, const jsi::Value &thisValue, const jsi::Value *args, size_t count) -> jsi::Value

#define HFN0 jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forAscii(rt, ""), 0, [](jsi::Runtime &rt, const jsi::Value &that, const jsi::Value *args, size_t count) -> jsi::Value
#define HFN(c1) jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forAscii(rt, ""), 0, [c1](jsi::Runtime &rt, const jsi::Value &that, const jsi::Value *args, size_t count) -> jsi::Value
#define HFN2(c1, c2) jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forAscii(rt, ""), 0, [c1, c2](jsi::Runtime &rt, const jsi::Value &that, const jsi::Value *args, size_t count) -> jsi::Value
#define HFN3(c1, c2, c3) jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forAscii(rt, ""), 0, [c1, c2, c3](jsi::Runtime &rt, const jsi::Value &that, const jsi::Value *args, size_t count) -> jsi::Value
