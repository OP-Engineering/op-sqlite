# Updating Libsql

# Submitting libsql updates

## Re-compile the C binary for iOS and Android

PR which originally introduced support for iOS/Android

[https://github.com/tursodatabase/libsql/pull/1423/files](https://github.com/tursodatabase/libsql/pull/1423/files)

You only need to navigate to the `bindings/c` folder and run the make scripts to generate the files

```bash
cd libsql/bindings/c
make ios
make android
```

Sqlcipher/Crypto support is disabled for now, just needs someone to put the time to get it to compile. The makefile then compiles libsql for iOS/Android, RN just consumes these modules natively but can be consumed by any iOS/Android app.

## Submit a PR to op-sqlite

Afterwards you will have a bunch of files on the `bindings/c/generated` folder. You need to replace the `libsql.xcframework` to `op-sqlite/ios/libsql.xcframework`. Then you need to copy the `jniLibs` to `op-sqlite/android/`.

As long as you have not changed the headers and bindings submitting a PR will be enough, there are automated tests that will build the app to make sure everything is compiling. If you did modify the headers you also need to update the file at `cpp/libsql/libsql.h` .
