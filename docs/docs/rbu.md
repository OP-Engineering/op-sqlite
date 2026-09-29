---
sidebar_position: 8
---

# Resumable bulk updates (RBU)

[SQLite RBU](https://sqlite.org/rbu.html) applies a prepared bulk update incrementally. A bounded update can save its state, stop, and resume in another process while readers continue to see the original database until the update reaches the commit stage.

OP-SQLite only applies an existing RBU database. It does not create RBU artifacts, run `sqldiff`, migrate schemas, or merge conflicting changes. The artifact must have been prepared externally for the exact base version of the target database.

## Enable RBU

RBU adds native code to SQLite and is disabled by default. Enable it in the application's `package.json`, then rebuild the native application (including `pod install` when using CocoaPods):

```json
{
  "op-sqlite": {
    "rbu": true
  }
}
```

This setting adds `SQLITE_ENABLE_RBU=1` to Android CMake, CocoaPods, and SwiftPM builds. The existing low-level configuration is also supported:

```json
{
  "op-sqlite": {
    "sqliteFlags": "-DSQLITE_ENABLE_RBU=1"
  }
}
```

The dedicated setting is recommended because it is discoverable and allows OP-SQLite to validate backend compatibility. Do not combine the two mechanisms. Like other SQLite extension flags, `SQLITE_ENABLE_RBU` is presence-based: `-DSQLITE_ENABLE_RBU=0` still compiles the extension. Omit the flag and set `rbu` to `false` to disable RBU.

RBU currently supports only OP-SQLite's bundled vanilla SQLite on Android and Apple platforms. It is unavailable with SQLCipher, libSQL, Turso, Apple's embedded/system SQLite (`iosSqlite`), the web backend, and the Node.js test facade. `isRBUEnabled()` reports the active native build capability. Configuring `rbu: true` with an unsupported native backend fails the native build with an explanatory error.

SQLCipher's amalgamation contains the optional RBU source, but RBU opens and inspects its internal target, update, and state connections before this API could configure encryption keys. Encrypted target and artifact behavior is therefore not proven or supported.

## Pause and resume

Close every ordinary OP-SQLite connection to the target before starting RBU. RBU owns its target connection for the duration of each call.

```ts
import { applyRBU, isRBUEnabled } from "@op-engineering/op-sqlite";

if (!isRBUEnabled()) {
  throw new Error("Rebuild OP-SQLite with RBU enabled");
}

const paths = {
  targetPath: "/absolute/path/catalog.sqlite",
  updatePath: "/absolute/path/catalog-update.sqlite",
  statePath: "/absolute/path/catalog-update-state.sqlite",
};

let result = await applyRBU({ ...paths, maxSteps: 100 });

while (result.status === "paused") {
  result = await applyRBU({ ...paths, maxSteps: 100 });
}
```

`applyRBU()` runs on a native background thread. `maxSteps` must be a positive safe integer and bounds the number of `sqlite3rbu_step()` calls made by that invocation. It defaults to 1000 so a single call cannot monopolize native-module teardown. Keep calling `applyRBU()` with the same paths while the result is `paused`.

The result contains:

- `status`: `paused` or `complete`;
- `steps`: work steps performed by this invocation;
- `progress`: SQLite's cumulative `sqlite3rbu_progress()` value (work units, not a percentage);
- `state`: `oal`, `move`, `checkpoint`, `done`, `error`, or `unknown`.

Calling `applyRBU()` again with the same completed update and preserved state returns `complete`; SQLite marks the artifact as fully applied.

## State and recovery

When `statePath` is provided, RBU creates a separate SQLite state database. Preserve the target database, RBU artifact, state database, and RBU-created sidecar files together until completion. Do not delete or replace the state database during an incomplete update; it is the resume cursor, and losing it can make the remaining sidecars inconsistent with a fresh application attempt.

When `statePath` is omitted, SQLite stores tables beginning with `rbu_` inside the update database. A separate state path is usually easier to manage when the artifact itself should remain immutable.

Closing after a bounded call persists a resumable checkpoint. Following a process termination or device restart, a new process may call `applyRBU()` with the same paths and continue from SQLite's most recently persisted state. As with SQLite RBU itself, this is crash recovery rather than distributed conflict resolution; power loss at an unlucky point can still surface a SQLite constraint or I/O error that requires replacing the artifact and its state.

## Target restrictions

Before applying an update:

- close ordinary OP-SQLite connections and prevent concurrent writers;
- ensure the target is not in WAL journal mode (for example, use `PRAGMA journal_mode=DELETE` before closing it);
- use an RBU artifact prepared for the exact target database version;
- keep the files on storage that supports SQLite's required locking and atomic filesystem operations.

RBU does not fire triggers or enforce foreign-key and `CHECK` constraints while applying changes. Review the full [SQLite RBU limitations](https://sqlite.org/rbu.html#rbu_update_limitations) when producing artifacts.

## Errors

Failures reject with `RBUError`. Its `code` property is the numeric SQLite result code and `message` contains SQLite's message plus OP-SQLite context.

```ts
import { applyRBU, RBUError } from "@op-engineering/op-sqlite";

try {
  await applyRBU(paths);
} catch (error) {
  if (error instanceof RBUError) {
    console.error(error.code, error.message);
  }
}
```

Missing target/update files, malformed RBU databases, invalid options, unavailable builds, and unsupported platforms all reject instead of creating a target or silently falling back.
