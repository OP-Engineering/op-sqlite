---
sidebar_position: 11
---

# API Changes

## 18.0.0

- **Breaking:** Removed `crsqlite` support entirely. The `crsqlite` key in the `op-sqlite` `package.json` config no longer has any effect, and the bundled `cr-sqlite` extension binaries have been removed from the package (iOS `crsqlite.xcframework`, Android `libcrsqlite` `.so`s). If you need CR-SQLite, load it yourself as a runtime extension via `loadExtension` — see [Loading Extensions](./api.md#loading-extensions).

## 17.2.0

- Added `failOnCreate` option to `open()`. When set to `true`, the database file must already exist; if it doesn't, `open()` throws instead of creating it. Implemented natively across all backends (plain SQLite3, SQLCipher, libsql and Turso). See the [Open Existing Only (failOnCreate)](./api.md#open-existing-only-failoncreate) section for usage.
- Removed support for combining `crsqlite` with `libsql`. Enabling both in `package.json` now fails the build (iOS podspec and Android Gradle) with a clear error instead of silently loading the extension. If you relied on this combination, drop one of the two flags.
