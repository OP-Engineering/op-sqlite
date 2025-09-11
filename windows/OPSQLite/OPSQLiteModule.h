#pragma once

#include <NativeModules.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Microsoft.ReactNative.h>

namespace winrt::OPSQLite::implementation
{
    REACT_MODULE(OPSQLiteModule, L"OPSQLite")
    struct OPSQLiteModule
    {
        REACT_INIT(Initialize)
        void Initialize(Microsoft::ReactNative::ReactContext const& reactContext) noexcept;

        REACT_METHOD(install, L"install")
        bool install() noexcept;

        REACT_METHOD(moveAssetsDatabase, L"moveAssetsDatabase")
        bool moveAssetsDatabase(std::string name, std::string extension) noexcept;

        REACT_GET_CONSTANTS(GetConstants)
        Microsoft::ReactNative::JSValueObject GetConstants() noexcept;

    private:
        Microsoft::ReactNative::ReactContext m_reactContext;
        bool m_jsiInstalled = false;
        static void InstallJSI(Microsoft::ReactNative::ReactContext const& reactContext) noexcept;
        static void ClearState() noexcept;
    };
}
