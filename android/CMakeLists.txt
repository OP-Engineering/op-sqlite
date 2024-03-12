project(OPSQLite)
cmake_minimum_required(VERSION 3.9.0)

set (PACKAGE_NAME "op-sqlite")
set (CMAKE_VERBOSE_MAKEFILE ON)
set (CMAKE_CXX_STANDARD 17)
set (BUILD_DIR ${CMAKE_SOURCE_DIR}/build)

 include_directories(
   ../cpp
   ../cpp/sqlcipher
 )

add_definitions(
  ${SQLITE_FLAGS}
)

add_library(
  ${PACKAGE_NAME}
  SHARED
  ../cpp/bridge.cpp
  ../cpp/bridge.h
  ../cpp/bindings.cpp
  ../cpp/bindings.h
  ../cpp/utils.h
  ../cpp/utils.cpp
  ../cpp/ThreadPool.h
  ../cpp/ThreadPool.cpp
  ../cpp/sqlbatchexecutor.h
  ../cpp/sqlbatchexecutor.cpp
  ../cpp/SmartHostObject.cpp
  ../cpp/SmartHostObject.h
  ../cpp/PreparedStatementHostObject.h
  ../cpp/PreparedStatementHostObject.cpp
  ../cpp/DumbHostObject.cpp
  ../cpp/DumbHostObject.h
  ../cpp/macros.h
  ../cpp/types.h
  cpp-adapter.cpp
)

if (OP_SQLITE_USE_SQLCIPHER)
  target_sources(${PACKAGE_NAME} PRIVATE ../cpp/sqlcipher/sqlite3.h ../cpp/sqlcipher/sqlite3.c)
  
  add_definitions(-DOP_SQLITE_USE_SQLCIPHER)
else()
  target_sources(${PACKAGE_NAME} PRIVATE ../cpp/sqlite3.h ../cpp/sqlite3.c)
endif()

set_target_properties(
  ${PACKAGE_NAME} PROPERTIES
  CXX_STANDARD 17
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON
)

find_package(ReactAndroid REQUIRED CONFIG)
find_package(fbjni REQUIRED CONFIG)

target_link_libraries(
  ${PACKAGE_NAME}
  fbjni::fbjni
  ReactAndroid::jsi
  ReactAndroid::turbomodulejsijni
  ReactAndroid::react_nativemodule_core
  android
)
