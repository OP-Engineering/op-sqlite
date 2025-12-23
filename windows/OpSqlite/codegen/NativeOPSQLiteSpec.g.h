
/*
 * This file is auto-generated from a NativeModule spec file in js.
 *
 * This is a C++ Spec class that should be used with MakeTurboModuleProvider to register native modules
 * in a way that also verifies at compile time that the native module matches the interface required
 * by the TurboModule JS spec.
 */
#pragma once
// clang-format off

// #include "NativeOPSQLiteDataTypes.g.h" before this file to use the generated type definition
#include <NativeModules.h>
#include <tuple>

namespace OpSqliteCodegen {

inline winrt::Microsoft::ReactNative::FieldMap GetStructInfo(OPSQLiteSpec_Constants*) noexcept {
    winrt::Microsoft::ReactNative::FieldMap fieldMap {
        {L"IOS_DOCUMENT_PATH", &OPSQLiteSpec_Constants::IOS_DOCUMENT_PATH},
        {L"IOS_LIBRARY_PATH", &OPSQLiteSpec_Constants::IOS_LIBRARY_PATH},
        {L"ANDROID_DATABASE_PATH", &OPSQLiteSpec_Constants::ANDROID_DATABASE_PATH},
        {L"ANDROID_FILES_PATH", &OPSQLiteSpec_Constants::ANDROID_FILES_PATH},
        {L"ANDROID_EXTERNAL_FILES_PATH", &OPSQLiteSpec_Constants::ANDROID_EXTERNAL_FILES_PATH},
    };
    return fieldMap;
}

struct OPSQLiteSpec : winrt::Microsoft::ReactNative::TurboModuleSpec {
  static constexpr auto constants = std::tuple{
      TypedConstant<OPSQLiteSpec_Constants>{0},
  };
  static constexpr auto methods = std::tuple{
      SyncMethod<bool() noexcept>{0, L"install"},
      SyncMethod<bool(std::string, std::string) noexcept>{1, L"moveAssetsDatabase"},
  };

  template <class TModule>
  static constexpr void ValidateModule() noexcept {
    constexpr auto constantCheckResults = CheckConstants<TModule, OPSQLiteSpec>();
    constexpr auto methodCheckResults = CheckMethods<TModule, OPSQLiteSpec>();

    REACT_SHOW_CONSTANT_SPEC_ERRORS(
          0,
          "OPSQLiteSpec_Constants",
          "    REACT_GET_CONSTANTS(GetConstants) OPSQLiteSpec_Constants GetConstants() noexcept {/*implementation*/}\n"
          "    REACT_GET_CONSTANTS(GetConstants) static OPSQLiteSpec_Constants GetConstants() noexcept {/*implementation*/}\n");

    REACT_SHOW_METHOD_SPEC_ERRORS(
          0,
          "install",
          "    REACT_SYNC_METHOD(install) bool install() noexcept { /* implementation */ }\n"
          "    REACT_SYNC_METHOD(install) static bool install() noexcept { /* implementation */ }\n");
    REACT_SHOW_METHOD_SPEC_ERRORS(
          1,
          "moveAssetsDatabase",
          "    REACT_SYNC_METHOD(moveAssetsDatabase) bool moveAssetsDatabase(std::string name, std::string extension) noexcept { /* implementation */ }\n"
          "    REACT_SYNC_METHOD(moveAssetsDatabase) static bool moveAssetsDatabase(std::string name, std::string extension) noexcept { /* implementation */ }\n");
  }
};

} // namespace OpSqliteCodegen
