---
sidebar_position: 10
---

# C++ Usage

In certain scenarios, you may prefer direct access to SQLite C++ code, allowing efficient database operations directly from C++ in a React Native environment, independent of JavaScript.

## Android CMake Integration

The package supports prefab publishing for Android, which allows you to access it from within your CMakeLists.txt file.

Add the following to your `CMakeLists.txt`:

```cmake
find_package(op-engineering_op-sqlite REQUIRED CONFIG)

# Link all libraries together
target_link_libraries(
        ${PACKAGE_NAME}
        ${LOG_LIB}
        android
        op-engineering_op-sqlite::op-sqlite
)
```

## Header File Inclusion

Due to platform differences, you need to include the SQLite header differently for Android and iOS:

### Example on how to include the headers in C++

```cpp
#ifdef __ANDROID__
#include <op-engineering_op-sqlite/sqlite3.h>
#include <op-engineering_op-sqlite/bridge.h>
#else
#include <op-sqlite/sqlite3.h>
#include <op-sqlite/bridge.h>
#endif
```
