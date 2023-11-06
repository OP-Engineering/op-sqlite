require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name         = "op-sqlite"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => "12.0", :osx => "10.7" }
  s.source       = { :git => "https://github.com/ospfranco/op-sqlite.git", :tag => "#{s.version}" }

  s.pod_target_xcconfig = {
    :GCC_PREPROCESSOR_DEFINITIONS => "HAVE_FULLFSYNC=1",
    :WARNING_CFLAGS => "-Wno-shorten-64-to-32 -Wno-comma -Wno-unreachable-code -Wno-conditional-uninitialized -Wno-deprecated-declarations",
    :USE_HEADERMAP => "No"
  }
  
  s.header_mappings_dir = "cpp"
  s.source_files = "ios/**/*.{h,hpp,m,mm}", "cpp/**/*.{h,hpp,cpp,c}"
  
  s.dependency "React-callinvoker"
  s.dependency "React"
  s.dependency "React-Core"

  s.xcconfig = {
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++17',
  }


  if ENV['OP_SQLITE_USE_PHONE_VERSION'] == '1' then
    s.exclude_files = "cpp/sqlite3.c", "cpp/sqlite3.h"
    s.library = "sqlite3"
  end
  
end
