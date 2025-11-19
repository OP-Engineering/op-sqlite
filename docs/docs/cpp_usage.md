---
sidebar_position: 10
---

# C++ Usage

It's possible to access the native C++ functions in case you want to directly call the underlying API without React-Native/JavaScript. This is useful in case you have native code and need to access sqlite functionality.

## Android

The package supports prefab publishing for Android, which allows you to access it from within your `CMakeLists.txt` file.

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

## Add header to your native code

Due to platform differences, you need to include the SQLite header differently for Android and iOS:

```cpp
#ifdef __ANDROID__
#include <op-engineering_op-sqlite/sqlite3.h>
#include <op-engineering_op-sqlite/bridge.h>
#else
#include <op-sqlite/sqlite3.h>
#include <op-sqlite/bridge.h>
#endif
```

If you are wondering how to use C++ code, on Android it's achieved via JNI/NDK code, on iOS you can use any Objective-C++ (`.mm`) file and import it directly. Swift integration might not work and will require setting the min swift version to `5.9`, it's not covered in this guide.
