#pragma once

#include "pch.h"
#include "resource.h"

#if __has_include("codegen/NativeOpSqliteDataTypes.g.h")
  #include "codegen/NativeOpSqliteDataTypes.g.h"
#endif
#include "codegen/NativeOpSqliteSpec.g.h"

#include "NativeModules.h"

namespace winrt::OpSqlite
{

// See https://microsoft.github.io/react-native-windows/docs/native-platform for help writing native modules

REACT_MODULE(OpSqlite)
struct OpSqlite
{
  using ModuleSpec = OpSqliteCodegen::OpSqliteSpec;

  REACT_INIT(Initialize)
  void Initialize(React::ReactContext const &reactContext) noexcept;

  REACT_SYNC_METHOD(multiply)
  double multiply(double a, double b) noexcept;

private:
  React::ReactContext m_context;
};

} // namespace winrt::OpSqlite