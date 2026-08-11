---
sidebar_position: 11
---

# API Changes

## 17.2.0

- Added `failOnCreate` option to `open()`. When set to `true`, the database file must already exist; if it doesn't, `open()` throws instead of creating it. Implemented natively across all backends (plain SQLite3, SQLCipher, libsql and Turso). See the [Open Existing Only (failOnCreate)](./api.md#open-existing-only-failoncreate) section for usage.
