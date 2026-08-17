---
sidebar_position: 7
---

# Turso Backend

Turso is supported as a backend in op-sqlite.

Enable it in your package.json:

```json
"op-sqlite": {
  "turso": true
}
```

You cannot use `turso` together with `sqlcipher` or `libsql` in the same build.

## Remote Database

Use `openRemote` for a purely remote connection:

```tsx
import { openRemote } from '@op-engineering/op-sqlite';

const remoteDb = openRemote({
  url: 'url',
  authToken: 'token',
});
```

## Local + Sync Database

Use `openSync` to create/open a local database that syncs with a remote Turso database:

```tsx
import { openSync } from '@op-engineering/op-sqlite';

const syncDb = openSync({
  name: 'myDb.sqlite',
  url: 'url',
  authToken: 'token',
  remoteEncryptionKey: 'optional-remote-encryption-key',
});
```

## Triggering Sync

Call `sync()` when you want to force a sync cycle:

```tsx
syncDb.sync();
```

The API shape is the same as libsql-backed remote/sync usage.
