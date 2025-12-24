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

REACT_MODULE(OpSqlite, L"OPSQLite")
struct OpSqlite
{
  using ModuleSpec = OpSqliteCodegen::OPSQLiteSpec;

  REACT_INIT(Initialize)
  void Initialize(React::ReactContext const &reactContext) noexcept;

  REACT_GET_CONSTANTS(GetConstants)
  OpSqliteCodegen::OPSQLiteSpec_Constants GetConstants() noexcept;

  REACT_SYNC_METHOD(install)
  bool install() noexcept;

  REACT_SYNC_METHOD(moveAssetsDatabase)
  bool moveAssetsDatabase(std::string name, std::string extension) noexcept;

private:
  React::ReactContext m_context;
  std::string m_basePath;
};

} // namespace winrt::OpSqlite