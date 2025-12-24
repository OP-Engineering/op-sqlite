#include "pch.h"

#include "OpSqlite.h"
#include "../../cpp/OPSqlite.hpp"

namespace winrt::OpSqlite
{

// See https://microsoft.github.io/react-native-windows/docs/native-platform for help writing native modules

void OpSqlite::Initialize(React::ReactContext const &reactContext) noexcept {
  m_context = reactContext;
  
  // Get the local app data folder for database storage
  try {
    auto localFolder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
    m_basePath = winrt::to_string(localFolder.Path());
  } catch (...) {
    // Fallback to current directory if ApplicationData is not available
    m_basePath = std::filesystem::current_path().string();
  }
}

OpSqliteCodegen::OPSQLiteSpec_Constants OpSqlite::GetConstants() noexcept {
  OpSqliteCodegen::OPSQLiteSpec_Constants constants;
  
  // For Windows, we use LocalFolder for database storage
  // Set all paths to the same base path since Windows doesn't have iOS/Android specific paths
  constants.IOS_DOCUMENT_PATH = "";
  constants.IOS_LIBRARY_PATH = "";
  constants.ANDROID_DATABASE_PATH = "";
  constants.ANDROID_FILES_PATH = "";
  constants.ANDROID_EXTERNAL_FILES_PATH = "";
  
  return constants;
}

bool OpSqlite::install() noexcept {
  try {
    // Get the JSI runtime synchronously
    // TryGetOrCreateContextRuntime returns a pointer to the JSI runtime
    facebook::jsi::Runtime* runtime = winrt::Microsoft::ReactNative::TryGetOrCreateContextRuntime(m_context);
    if (!runtime) {
      return false;
    }
    
    auto callInvoker = m_context.CallInvoker();
    if (!callInvoker) {
      return false;
    }
    
    // Call the core opsqlite install function synchronously
    opsqlite::install(
      *runtime,
      callInvoker,
      m_basePath.c_str(),
      "",  // crsqlite_path - not supported on Windows yet
      ""   // sqlite_vec_path - not supported on Windows yet
    );
    
    return true;
  } catch (const std::exception& e) {
    // Log error if needed
    return false;
  } catch (...) {
    return false;
  }
}

bool OpSqlite::moveAssetsDatabase(std::string name, std::string extension) noexcept {
  try {
    // On Windows, assets are typically bundled differently
    // For now, this is a placeholder that returns false
    // A proper implementation would need to locate the asset in the app package
    // and copy it to the local folder
    
    std::string sourcePath = name;
    if (!extension.empty()) {
      sourcePath += "." + extension;
    }
    
    std::string destPath = m_basePath + "\\" + name;
    if (!extension.empty()) {
      destPath += "." + extension;
    }
    
    // Check if destination already exists
    if (std::filesystem::exists(destPath)) {
      return true;
    }
    
    // Try to copy from assets folder (this path may need adjustment based on app structure)
    // Windows UWP apps have assets in the app installation folder
    try {
      auto installFolder = winrt::Windows::ApplicationModel::Package::Current().InstalledLocation();
      auto assetsPath = winrt::to_string(installFolder.Path()) + "\\assets\\sqlite\\" + name;
      if (!extension.empty()) {
        assetsPath += "." + extension;
      }
      
      if (std::filesystem::exists(assetsPath)) {
        std::filesystem::copy_file(assetsPath, destPath, std::filesystem::copy_options::skip_existing);
        return true;
      }
    } catch (...) {
      // Asset copy failed
    }
    
    return false;
  } catch (...) {
    return false;
  }
}

} // namespace winrt::OpSqlite