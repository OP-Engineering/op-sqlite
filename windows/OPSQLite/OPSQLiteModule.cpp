#include "pch.h"
#include "OPSQLiteModule.h"
#include "../../cpp/bindings.h"
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.ApplicationModel.h>
#include <filesystem>
#include <string>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Microsoft::ReactNative;

namespace winrt::OPSQLite::implementation
{
    void OPSQLiteModule::Initialize(ReactContext const& reactContext) noexcept
    {
        m_reactContext = reactContext;
    }

    bool OPSQLiteModule::install() noexcept
    {
        if (m_jsiInstalled) {
            return true;
        }

        try {
            InstallJSI(m_reactContext);
            m_jsiInstalled = true;
            return true;
        }
        catch (...) {
            return false;
        }
    }

    bool OPSQLiteModule::moveAssetsDatabase(std::string name, std::string extension) noexcept
    {
        try {
            auto localFolder = ApplicationData::Current().LocalFolder();
            auto localPath = localFolder.Path();
            
            // Construct database file name
            std::string dbFileName = name;
            if (!extension.empty() && extension != "db") {
                dbFileName += "." + extension;
            } else if (extension.empty()) {
                dbFileName += ".db";
            }

            // Get the source path from the app package
            auto installFolder = Windows::ApplicationModel::Package::Current().InstalledLocation();
            auto assetsFolder = installFolder.GetFolderAsync(L"Assets").get();
            auto sourceFile = assetsFolder.GetFileAsync(winrt::to_hstring(dbFileName)).get();
            
            // Copy to local folder
            auto destPath = localPath + L"\\" + to_hstring(dbFileName);
            sourceFile.CopyAsync(localFolder, to_hstring(dbFileName), 
                NameCollisionOption::ReplaceExisting).get();
            
            return true;
        }
        catch (...) {
            return false;
        }
    }

    JSValueObject OPSQLiteModule::GetConstants() noexcept
    {
        JSValueObject constants;
        
        try {
            auto localFolder = ApplicationData::Current().LocalFolder();
            auto localPath = localFolder.Path();
            auto tempFolder = ApplicationData::Current().TemporaryFolder();
            auto tempPath = tempFolder.Path();
            auto roamingFolder = ApplicationData::Current().RoamingFolder();
            auto roamingPath = roamingFolder.Path();

            constants["WINDOWS_LOCAL_DATA_PATH"] = winrt::to_string(localPath);
            constants["WINDOWS_TEMP_PATH"] = winrt::to_string(tempPath);
            constants["WINDOWS_ROAMING_DATA_PATH"] = winrt::to_string(roamingPath);
            
            // Add empty paths for other platforms to maintain API compatibility
            constants["ANDROID_DATABASE_PATH"] = "";
            constants["ANDROID_FILES_PATH"] = "";
            constants["ANDROID_EXTERNAL_FILES_PATH"] = "";
            constants["IOS_DOCUMENT_PATH"] = "";
            constants["IOS_LIBRARY_PATH"] = "";
        }
        catch (...) {
            // Return empty paths if we can't get them
        }

        return constants;
    }

    void OPSQLiteModule::InstallJSI(ReactContext const& reactContext) noexcept
    {
        reactContext.CallJSFunction(L"RCTDeviceEventEmitter", L"emit", 
            JSValueArray{ L"OPSQLiteInstalling" });

        auto jsDispatcher = reactContext.JSDispatcher();
        auto jsRuntime = reactContext.JSRuntime();
        
        jsDispatcher.Post([jsRuntime, reactContext]() {
            auto& runtime = jsRuntime.Get();
            auto callInvoker = reactContext.CallInvoker();
            
            // Get the local folder path for database storage
            auto localFolder = ApplicationData::Current().LocalFolder();
            auto localPath = winrt::to_string(localFolder.Path());
            
            // Install the JSI bindings
            // For Windows, we don't have crsqlite or sqlite_vec paths yet
            // These can be added later when Windows builds are available
            opsqlite::install(runtime, callInvoker, localPath.c_str(), "", "");
            
            // Register cleanup handler for reload scenarios
            reactContext.AddJavaScriptContextExtension(
                L"OPSQLite",
                [](IJSValueWriter const& writer) {
                    // This is called when the JS context is being destroyed
                    opsqlite::invalidate();
                });
        });
    }

    void OPSQLiteModule::ClearState() noexcept
    {
        opsqlite::invalidate();
    }
}
