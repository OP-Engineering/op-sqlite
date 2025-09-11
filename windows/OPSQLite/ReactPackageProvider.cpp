#include "pch.h"
#include "ReactPackageProvider.h"
#include "OPSQLiteModule.h"

using namespace winrt::Microsoft::ReactNative;

namespace winrt::OPSQLite::implementation
{
    void ReactPackageProvider::CreatePackage(IReactPackageBuilder const& packageBuilder) noexcept
    {
        AddAttributedModules(packageBuilder, true);
    }
}

namespace winrt::OPSQLite::factory_implementation
{
    struct ReactPackageProvider : ReactPackageProviderT<ReactPackageProvider, implementation::ReactPackageProvider> {};
}

IReactPackageProvider MakeReactPackageProvider()
{
    return winrt::OPSQLite::factory_implementation::ReactPackageProvider();
}
