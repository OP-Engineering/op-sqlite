---
sidebar_position: 11
---

# API Changes

## 18.1.0

- Errors coming from SQLite now carry their result codes: rejected/thrown `Error`s from `execute`, `executeSync`, `executeRaw`, `executeRawSync`, `executeBatch`, `prepareStatement`, `attach`, `detach`, `loadExtension` and `open` expose `code` (primary) and `extendedCode` (extended), and both are repeated in the message. Only the plain SQLite3 and SQLCipher backends report them; libsql, Turso, web and node only expose a message. See [Error codes](./api.md#error-codes).

## 18.0.0

- **Breaking:** Removed `crsqlite` support entirely. The `crsqlite` key in the `op-sqlite` `package.json` config no longer has any effect, and the bundled `cr-sqlite` extension binaries have been removed from the package (iOS `crsqlite.xcframework`, Android `libcrsqlite` `.so`s). If you need CR-SQLite, load it yourself as a runtime extension via `loadExtension` — see [Loading Extensions](./api.md#loading-extensions).

## 17.2.0

- Added `failOnCreate` option to `open()`. When set to `true`, the database file must already exist; if it doesn't, `open()` throws instead of creating it. Implemented natively across all backends (plain SQLite3, SQLCipher, libsql and Turso). See the [Open Existing Only (failOnCreate)](./api.md#open-existing-only-failoncreate) section for usage.
- Removed support for combining `crsqlite` with `libsql`. Enabling both in `package.json` now fails the build (iOS podspec and Android Gradle) with a clear error instead of silently loading the extension. If you relied on this combination, drop one of the two flags.
