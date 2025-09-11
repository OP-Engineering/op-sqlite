cmake_minimum_required(VERSION 3.13)
project(OPSQLite)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find React Native Windows
find_package(ReactNativeWindows REQUIRED)

# Source files
set(SOURCES
    pch.cpp
    OPSQLiteModule.cpp
    ReactPackageProvider.cpp
    ../../cpp/bindings.cpp
    ../../cpp/bridge.cpp
    ../../cpp/DBHostObject.cpp
    ../../cpp/DumbHostObject.cpp
    ../../cpp/SmartHostObject.cpp
    ../../cpp/PreparedStatementHostObject.cpp
    ../../cpp/OPThreadPool.cpp
    ../../cpp/utils.cpp
    ../../cpp/sqlite3.c
)

# Header files
set(HEADERS
    pch.h
    OPSQLiteModule.h
    ReactPackageProvider.h
    ../../cpp/bindings.h
    ../../cpp/bridge.h
    ../../cpp/DBHostObject.h
    ../../cpp/DumbHostObject.h
    ../../cpp/SmartHostObject.h
    ../../cpp/PreparedStatementHostObject.h
    ../../cpp/OPThreadPool.h
    ../../cpp/sqlite3.h
    ../../cpp/types.h
    ../../cpp/utils.h
    ../../cpp/logs.h
    ../../cpp/macros.h
)

# Create the DLL
add_library(OPSQLite SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(OPSQLite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp
    ${ReactNativeWindows_INCLUDE_DIRS}
)

# Compile definitions
target_compile_definitions(OPSQLite PRIVATE
    _WINDOWS
    _USRDLL
    HAVE_FULLFSYNC=1
    SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION=1
)

# Release optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(OPSQLite PRIVATE
        NDEBUG
        SQLITE_DQS=0
        SQLITE_DEFAULT_MEMSTATUS=0
        SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
        SQLITE_LIKE_DOESNT_MATCH_BLOBS=1
        SQLITE_MAX_EXPR_DEPTH=0
        SQLITE_OMIT_DEPRECATED=1
        SQLITE_OMIT_PROGRESS_CALLBACK=1
        SQLITE_OMIT_SHARED_CACHE=1
        SQLITE_USE_ALLOCA=1
        SQLITE_THREADSAFE=1
    )
    
    set_target_properties(OPSQLite PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION TRUE
    )
endif()

# Link libraries
target_link_libraries(OPSQLite
    ${ReactNativeWindows_LIBRARIES}
)

# Precompiled headers
target_precompile_headers(OPSQLite PRIVATE pch.h)

# Export symbols
set_target_properties(OPSQLite PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

# Module definition file
set_target_properties(OPSQLite PROPERTIES
    LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/OPSQLite.def"
)

# Install rules
install(TARGETS OPSQLite
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
