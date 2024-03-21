require "json"

log_message = lambda do |message|
  puts "\e[34m#{message}\e[0m"
end

package = JSON.parse(File.read(File.join(__dir__, "package.json")))
folly_compiler_flags = '-DFOLLY_NO_CONFIG -DFOLLY_MOBILE=1 -DFOLLY_USE_LIBCPP=1 -Wno-comma -Wno-shorten-64-to-32'
fabric_enabled = ENV['RCT_NEW_ARCH_ENABLED'] == '1'

Pod::Spec.new do |s|
  s.name         = "op-sqlite"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => "13.0", :osx => "10.15" }
  s.source       = { :git => "https://github.com/op-engineering/op-sqlite.git", :tag => "#{s.version}" }
  
  # s.header_mappings_dir = "cpp"
  s.source_files = "ios/**/*.{h,m,mm}", "cpp/**/*.{h,cpp,c}"

  xcconfig = {
    :GCC_PREPROCESSOR_DEFINITIONS => "HAVE_FULLFSYNC=1",
    :WARNING_CFLAGS => "-Wno-shorten-64-to-32 -Wno-comma -Wno-unreachable-code -Wno-conditional-uninitialized -Wno-deprecated-declarations",
    :USE_HEADERMAP => "No",
    :CLANG_CXX_LANGUAGE_STANDARD => "c++17",
  }
  
  if ENV['OP_SQLITE_USE_SQLCIPHER'] == '1' then
    log_message.call("[OP-SQLITE] using SQLCipher! 🔒")
    s.exclude_files = "cpp/sqlite3.c", "cpp/sqlite3.h"
    xcconfig[:GCC_PREPROCESSOR_DEFINITIONS] += " OP_SQLITE_USE_SQLCIPHER=1 HAVE_FULLFSYNC=1 SQLITE_HAS_CODEC SQLITE_TEMP_STORE=2"
    s.dependency "OpenSSL-Universal"
  else
    log_message.call("[OP-SQLITE] using vanilla SQLite! 📦")
    s.exclude_files = "cpp/sqlcipher/sqlite3.c", "cpp/sqlcipher/sqlite3.h"
  end
  
  s.dependency "React-callinvoker"
  s.dependency "React"
  if fabric_enabled then
    install_modules_dependencies(s)
  else
    s.dependency "React-Core"
  end

  other_cflags = '-DSQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION=1'

  optimizedCflags = other_cflags + '$(inherited) -DSQLITE_DQS=0 -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1 -DSQLITE_LIKE_DOESNT_MATCH_BLOBS=1 -DSQLITE_MAX_EXPR_DEPTH=0 -DSQLITE_OMIT_DEPRECATED=1 -DSQLITE_OMIT_PROGRESS_CALLBACK=1 -DSQLITE_OMIT_SHARED_CACHE=1 -DSQLITE_USE_ALLOCA=1'

  if ENV['OP_SQLITE_USE_PHONE_VERSION'] == '1' then
    log_message.call("[OP-SQLITE] using iOS embedded SQLite! 📱")
    xcconfig[:GCC_PREPROCESSOR_DEFINITIONS] += " OP_SQLITE_USE_PHONE_VERSION=1"
    s.exclude_files = "cpp/sqlite3.c", "cpp/sqlite3.h"
    s.library = "sqlite3"
  end

  if ENV['OP_SQLITE_PERF'] == '1' then
    log_message.call("[OP-SQLITE] performance mode enabled! 🚀")
    xcconfig[:OTHER_CFLAGS] = optimizedCflags + ' -DSQLITE_THREADSAFE=0 '
  end

  if ENV['OP_SQLITE_PERF'] == '2' then
    log_message.call("[OP-SQLITE] (thread safe) performance mode enabled! 🚀")
    xcconfig[:OTHER_CFLAGS] = optimizedCflags + ' -DSQLITE_THREADSAFE=1 '
  end

  s.pod_target_xcconfig = xcconfig
  
end
