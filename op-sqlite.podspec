require "json"
require_relative "./generate_tokenizers_header_file"

log_message = lambda do |message|
  puts "\e[34m#{message}\e[0m"
end

# In the sample app the dir is not inside of node_modules
is_user_app = __dir__.include?("node_modules")
package = JSON.parse(File.read(File.join(__dir__, "package.json")))
app_package = nil
package_json_path = nil

# When installed on user node_modules lives inside node_modules/@op-engineering/op-sqlite
# Find the users package.json by searching up through parent directories
if is_user_app
  current_dir = File.dirname(File.expand_path(__dir__))
  
  loop do
    package_path = File.join(current_dir, "package.json")
    if File.exist?(package_path)
      package_json_path = package_path
      break
    end

    break if File.dirname(current_dir) == current_dir  # reached filesystem root
    current_dir = parent_dir
  end
  
  raise "package.json not found? It's needed to read any op-sqlite config (if available)" if package_json_path.nil?
else
  package_json_path = File.join(__dir__, "example", "package.json")
end

app_package = JSON.parse(File.read(package_json_path))

op_sqlite_config = app_package["op-sqlite"]
use_sqlcipher = false
use_crsqlite = false
use_libsql = false
performance_mode = false
phone_version = false
sqlite_flags = ""
fts5 = false
rtree = false
use_sqlite_vec = false
tokenizers = []

if(op_sqlite_config != nil)
  use_sqlcipher = op_sqlite_config["sqlcipher"] == true
  use_crsqlite = op_sqlite_config["crsqlite"] == true
  use_libsql = op_sqlite_config["libsql"] == true
  performance_mode = op_sqlite_config["performanceMode"] || false
  phone_version = op_sqlite_config["iosSqlite"] == true
  sqlite_flags = op_sqlite_config["sqliteFlags"] || ""
  fts5 = op_sqlite_config["fts5"] == true
  rtree = op_sqlite_config["rtree"] == true
  use_sqlite_vec = op_sqlite_config["sqliteVec"] == true
  tokenizers = op_sqlite_config["tokenizers"] || []
end

if phone_version then
  if use_sqlcipher then
    raise "SQLCipher is not supported with phone version. It cannot load extensions."
  end

  if use_crsqlite then
    raise "CRSQLite is not supported with phone version. It cannot load extensions."
  end

  if rtree then
    raise "RTree is not supported with phone version. It cannot load extensions."
  end

  if use_sqlite_vec then
    raise "SQLite Vec is not supported with phone version. It cannot load extensions."
  end
end

Pod::Spec.new do |s|
  s.name         = "op-sqlite"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => min_ios_version_supported, :tvos => "13.0", :osx => "10.15", :visionos => "1.0" }
  s.source       = { :git => "https://github.com/op-engineering/op-sqlite.git", :tag => "#{s.version}" }

  log_message.call("[OP-SQLITE] Configuration found at #{package_json_path}")

  install_modules_dependencies(s)
  
  # Base source files
  source_files = Dir.glob("ios/**/*.{h,hpp,m,mm}") + Dir.glob("cpp/**/*.{hpp,h,cpp,c}")

  # Set the path to the `c_sources` directory based on environment
  if is_user_app
    c_sources_dir = File.join("..", "..", "..", "c_sources")
  else
    c_sources_dir = File.join("example", "c_sources")
  end

  if tokenizers.any?
    generate_tokenizers_header_file(tokenizers, File.join(c_sources_dir, "tokenizers.h"))
    FileUtils.cp_r(c_sources_dir, __dir__)
    # # Add all .h and .c files from the `c_sources` directory
    source_files += Dir.glob(File.join("c_sources", "**/*.{h,cpp}"))
  end

  # Assign the collected source files to `s.source_files`
  s.source_files = source_files
  
  xcconfig = {
    :GCC_PREPROCESSOR_DEFINITIONS => "",
    :CLANG_CXX_LANGUAGE_STANDARD => "c++20",
    :GCC_OPTIMIZATION_LEVEL => "2",
  }
  
  exclude_files = []
  
  if use_sqlcipher then
    log_message.call("[OP-SQLITE] using SQLCipher")
    exclude_files += ["cpp/sqlite3.c", "cpp/sqlite3.h", "cpp/libsql/bridge.c", "cpp/libsql/bridge.h", "cpp/libsql/bridge.cpp", "cpp/libsql/libsql.h", "ios/libsql.xcframework/**/*"]
    xcconfig[:GCC_PREPROCESSOR_DEFINITIONS] += " OP_SQLITE_USE_SQLCIPHER=1 HAVE_FULLFSYNC=1 SQLITE_HAS_CODEC SQLITE_TEMP_STORE=3 SQLITE_EXTRA_INIT=sqlcipher_extra_init SQLITE_EXTRA_SHUTDOWN=sqlcipher_extra_shutdown"
    s.dependency "OpenSSL-Universal"    
  elsif use_libsql then
    log_message.call("[OP-SQLITE] using libsql. Please contact turso (via Discord) for libsql issues")
    exclude_files += ["cpp/sqlite3.c", "cpp/sqlite3.h", "cpp/sqlcipher/sqlite3.c", "cpp/sqlcipher/sqlite3.h", "cpp/bridge.h", "cpp/bridge.cpp"]
  else
    log_message.call("[OP-SQLITE] using pure SQLite")
    exclude_files += ["cpp/sqlcipher/sqlite3.c", "cpp/sqlcipher/sqlite3.h", "cpp/libsql/bridge.c", "cpp/libsql/bridge.h", "cpp/libsql/bridge.cpp", "cpp/libsql/libsql.h", "ios/libsql.xcframework/**/*"]
  end

   # Exclude xcframeworks that aren't being used
  if !use_crsqlite then
    exclude_files += ["ios/crsqlite.xcframework/**/*"]
  end

  if !use_sqlite_vec then
    exclude_files += ["ios/sqlitevec.xcframework/**/*"]
  end
  
  other_cflags = '$(inherited) -DSQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION=1 -DHAVE_USLEEP=1 -DSQLITE_ENABLE_LOCKING_STYLE=0'
  optimizedCflags = ' -DSQLITE_DQS=0 -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1 -DSQLITE_LIKE_DOESNT_MATCH_BLOBS=1 -DSQLITE_MAX_EXPR_DEPTH=0 -DSQLITE_OMIT_DEPRECATED=1 -DSQLITE_OMIT_PROGRESS_CALLBACK=1 -DSQLITE_OMIT_SHARED_CACHE=1 -DSQLITE_USE_ALLOCA=1 -DSQLITE_STRICT_SUBTYPE=1'
  frameworks = []

  if fts5 then
    xcconfig[:GCC_PREPROCESSOR_DEFINITIONS] += " SQLITE_ENABLE_FTS5=1"
  end

  if rtree then
    xcconfig[:GCC_PREPROCESSOR_DEFINITIONS] += " SQLITE_ENABLE_RTREE=1"
  end
 
  if phone_version then
    log_message.call("[OP-SQLITE] using iOS embedded SQLite 📱")
    xcconfig[:GCC_PREPROCESSOR_DEFINITIONS] += " OP_SQLITE_USE_PHONE_VERSION=1"
    exclude_files += ["cpp/sqlite3.c", "cpp/sqlite3.h"]
    s.library = "sqlite3"
  end

  if performance_mode then
    log_message.call("[OP-SQLITE] Performance mode enabled")
    other_cflags += optimizedCflags
  end

  if use_crsqlite then
    log_message.call("[OP-SQLITE] using CRQSQLite 🤖")
    xcconfig[:GCC_PREPROCESSOR_DEFINITIONS] += " OP_SQLITE_USE_CRSQLITE=1"
    frameworks.push("ios/crsqlite.xcframework")
  end

  if use_sqlite_vec then
    log_message.call("[OP-SQLITE] using Sqlite Vec ↗️")
    xcconfig[:GCC_PREPROCESSOR_DEFINITIONS] += " OP_SQLITE_USE_SQLITE_VEC=1"
    frameworks.push("ios/sqlitevec.xcframework")
  end

  if use_libsql then
    xcconfig[:GCC_PREPROCESSOR_DEFINITIONS] += " OP_SQLITE_USE_LIBSQL=1"
    if use_crsqlite then
      frameworks = ["ios/libsql.xcframework", "ios/crsqlite.xcframework"]
    else
      frameworks = ["ios/libsql.xcframework"]
    end
  end

  if sqlite_flags != "" then
    log_message.call("[OP-SQLITE] Detected custom SQLite flags: #{sqlite_flags}")
    other_cflags += " #{sqlite_flags}"
  end

  if tokenizers.any? then
    log_message.call("[OP_SQLITE] Tokenizers enabled: #{tokenizers}")
    if is_user_app then
      other_cflags += " -DTOKENIZERS_HEADER_PATH=\\\"../c_sources/tokenizers.h\\\""
    else 
      other_cflags += " -DTOKENIZERS_HEADER_PATH=\\\"../example/c_sources/tokenizers.h\\\""
    end
  end

  xcconfig[:OTHER_CFLAGS] = other_cflags
  s.pod_target_xcconfig = xcconfig
  s.vendored_frameworks = frameworks
  s.exclude_files = exclude_files
end
