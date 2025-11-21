project(OPSQLite)
cmake_minimum_required(VERSION 3.9.0)

set (PACKAGE_NAME "op-sqlite")

include_directories(
  ../cpp
  ../cpp/sqlcipher
  ../cpp/libsql
)

add_definitions(
  ${SQLITE_FLAGS}
)

add_library(
  ${PACKAGE_NAME}
  SHARED
  ../cpp/bindings.cpp
  ../cpp/utils.cpp
  ../cpp/OPThreadPool.cpp
  ../cpp/SmartHostObject.cpp
  ../cpp/PreparedStatementHostObject.cpp
  ../cpp/DumbHostObject.cpp
  ../cpp/DBHostObject.cpp
  cpp-adapter.cpp
)

if (USE_SQLCIPHER)
  target_sources(${PACKAGE_NAME} PRIVATE ../cpp/sqlcipher/sqlite3.h ../cpp/sqlcipher/sqlite3.c ../cpp/bridge.cpp ../cpp/bridge.h)

  add_definitions(
    -DOP_SQLITE_USE_SQLCIPHER=1
    -DSQLITE_HAS_CODEC
    -DSQLITE_TEMP_STORE=3
    -DSQLITE_EXTRA_INIT=sqlcipher_extra_init
    -DSQLITE_EXTRA_SHUTDOWN=sqlcipher_extra_shutdown
  )
  
  find_package(openssl REQUIRED CONFIG)
elseif (USE_LIBSQL)
  target_sources(${PACKAGE_NAME} PRIVATE ../cpp/libsql/bridge.cpp)
  
  add_definitions(
    -DOP_SQLITE_USE_LIBSQL=1
  )
else()
 target_sources(${PACKAGE_NAME} PRIVATE ../cpp/sqlite3.h ../cpp/sqlite3.c ../cpp/bridge.cpp ../cpp/bridge.h)
endif()

if (USE_CRSQLITE)
  add_definitions(
    -DOP_SQLITE_USE_CRSQLITE=1
  )
endif()

if (USE_SQLITE_VEC)
  add_definitions(
    -DOP_SQLITE_USE_SQLITE_VEC=1
  )
endif()

find_package(ReactAndroid REQUIRED CONFIG)
find_package(fbjni REQUIRED CONFIG)
find_library(LOG_LIB log)

# Add user defined files
if (USER_DEFINED_SOURCE_FILES)
  target_sources(${PACKAGE_NAME} PRIVATE ${USER_DEFINED_SOURCE_FILES})

  add_definitions("-DTOKENIZERS_HEADER_PATH=\"${USER_DEFINED_TOKENIZERS_HEADER_PATH}\"")
endif()

if (USE_SQLCIPHER)
  target_link_libraries(
    ${PACKAGE_NAME}
    ${LOG_LIB}
    ReactAndroid::reactnative
    ReactAndroid::jsi
    fbjni::fbjni
    openssl::crypto
  )
elseif (USE_LIBSQL)
  cmake_path(SET LIBSQL ${CMAKE_CURRENT_SOURCE_DIR}/jniLibs/${ANDROID_ABI}/libsql_experimental.a NORMALIZE)
  add_library(libsql STATIC IMPORTED)
  set_target_properties(libsql PROPERTIES IMPORTED_LOCATION ${LIBSQL})

  target_link_libraries(
    ${PACKAGE_NAME}
    ${LOG_LIB}
    ReactAndroid::reactnative
    ReactAndroid::jsi
    fbjni::fbjni
    libsql
  )
else ()
  target_link_libraries(
    ${PACKAGE_NAME}
    ${LOG_LIB}
    ReactAndroid::reactnative
    ReactAndroid::jsi
    fbjni::fbjni
  )
endif()