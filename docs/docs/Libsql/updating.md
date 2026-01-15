# Updating Libsql

## Re-compile the C binary for iOS and Android

Navigate to `libsql/bindings/c` folder and run:

```bash
make ios
make android
```

## Submit a PR

Afterwards you will have a bunch of files on the `bindings/c/generated` folder. You need to replace the files inside `opsqlite/ios/libsql_experimental.xcframework` (the binaries and the headers) and the `op-sqlite/android/jniLibs` folder with the generated files.

As long as you have not changed the headers and bindings submitting a PR will be enough, there are automated tests that will build the app to make sure everything is compiling.
