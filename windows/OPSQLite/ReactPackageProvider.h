#pragma once

#include <winrt/Microsoft.ReactNative.h>

using namespace winrt::Microsoft::ReactNative;

namespace winrt::OPSQLite::implementation
{
    struct ReactPackageProvider : winrt::implements<ReactPackageProvider, IReactPackageProvider>
    {
        void CreatePackage(IReactPackageBuilder const& packageBuilder) noexcept;
    };
}

IReactPackageProvider MakeReactPackageProvider();
