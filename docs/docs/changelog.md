---
sidebar_position: 11
---

# API Changes

## 19.0.0

- Errors coming from SQLite now carry their result codes: rejected/thrown `Error`s from `execute`, `executeSync`, `executeRaw`, `executeRawSync`, `executeBatch`, `prepareStatement`, `attach`, `detach`, `loadExtension` and `open` expose `code` (primary) and `extendedCode` (extended), and both are repeated in the message. Only the plain SQLite3 and SQLCipher backends report them; libsql, Turso, web and node only expose a message. See [Error codes](./api.md#error-codes).
- `executeBatch`, `transaction` and `loadFile` now always reject with the error that made them fail. Before, when SQLite had already rolled the transaction back on its own (`RAISE(ROLLBACK)` in a trigger, or a `COMMIT` failing with `SQLITE_FULL`/`SQLITE_IOERR`), the wrapper's `ROLLBACK` failed with `cannot rollback - no transaction is active` and that error replaced the real one. If the connection is left inside the transaction after a failed `ROLLBACK`, a new error saying so is thrown instead. In `transaction`, a failed `ROLLBACK` is attached to the original error as `rollbackError`, and in the stuck case the original error is kept as `cause`.
- The `BEGIN`/`COMMIT`/`ROLLBACK` around `executeBatch` now run natively, in the same background call as the batch, instead of as separate calls from JS.
- **Breaking:** Removed `executeBatchSync`. Its only difference was running `BEGIN`/`COMMIT` synchronously on the JS thread, and those now run natively for `executeBatch`. Use `executeBatch` instead.
- `tx.rollback()` no longer throws when SQLite has already rolled the transaction back (not on libsql, which cannot report the transaction state).
- libsql: errors raised while a statement runs (constraint violations, `RAISE()` in a trigger, a failing `COMMIT`...) are now thrown. They used to be printed to stderr and dropped, so the call resolved as if it had succeeded. Only errors raised while preparing the statement, such as a syntax error or a missing table, were reported.

## 18.0.0

- **Breaking:** Removed `crsqlite` support entirely. The `crsqlite` key in the `op-sqlite` `package.json` config no longer has any effect, and the bundled `cr-sqlite` extension binaries have been removed from the package (iOS `crsqlite.xcframework`, Android `libcrsqlite` `.so`s). If you need CR-SQLite, load it yourself as a runtime extension via `loadExtension` — see [Loading Extensions](./api.md#loading-extensions).

## 17.2.0

- Added `failOnCreate` option to `open()`. When set to `true`, the database file must already exist; if it doesn't, `open()` throws instead of creating it. Implemented natively across all backends (plain SQLite3, SQLCipher, libsql and Turso). See the [Open Existing Only (failOnCreate)](./api.md#open-existing-only-failoncreate) section for usage.
- Removed support for combining `crsqlite` with `libsql`. Enabling both in `package.json` now fails the build (iOS podspec and Android Gradle) with a clear error instead of silently loading the extension. If you relied on this combination, drop one of the two flags.
