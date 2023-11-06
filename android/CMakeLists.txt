project(OPSQLite)
cmake_minimum_required(VERSION 3.9.0)

set (PACKAGE_NAME "op-sqlite")
set (CMAKE_VERBOSE_MAKEFILE ON)
set (CMAKE_CXX_STANDARD 17)
set (BUILD_DIR ${CMAKE_SOURCE_DIR}/build)

include_directories(
  ../cpp
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
  ../cpp/sqlite3.h
  ../cpp/sqlite3.c
  ../cpp/utils.h
  ../cpp/utils.cpp
  ../cpp/ThreadPool.h
  ../cpp/ThreadPool.cpp
  ../cpp/sqlbatchexecutor.h
  ../cpp/sqlbatchexecutor.cpp
  ../cpp/DynamicHostObject.cpp
  ../cpp/DynamicHostObject.h
  ../cpp/macros.h
  cpp-adapter.cpp
)

set_target_properties(
  ${PACKAGE_NAME} PROPERTIES
  CXX_STANDARD 17
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON
)

find_package(ReactAndroid REQUIRED CONFIG)
find_package(fbjni REQUIRED CONFIG)
find_library(LOG_LIB log)

target_link_libraries(
  ${PACKAGE_NAME}
  ${LOG_LIB}
  fbjni::fbjni
  ReactAndroid::jsi
  ReactAndroid::turbomodulejsijni
  ReactAndroid::react_nativemodule_core
  android
)
