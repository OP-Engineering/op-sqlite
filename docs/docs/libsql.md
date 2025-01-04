---
sidebar_position: 6
---

# Libsql Support

Libsql is supported as a “sqlite” implementation. You can just turn it on by modifying your package.json:

```json
"op-sqlite": {
	"libsql": true
}
```

You cannot use sqlcipher and libsql at the same time, only turn on one or the other.

Unlike regular sqlite, `libsql` does not support certain features but adds others. Otherwise the API is the same as regular use with sqlite.

## Remote databases

Libsql allows you to connect to remote turso databases:

```tsx
import { openRemote } from '@op-engineering/op-sqlite';

const remoteDb = openRemote({
  url: 'url',
  authToken: 'token',
});
```

This is a purely remote database. You can however create a local database that syncs to a remote database:

```tsx
import { openSync } from '@op-engineering/op-sqlite';

const remoteDb = openSync({
  name: 'myDb.sqlite',
  url: 'url',
  authToken: 'token',
  syncInterval: 1, // Optional, in seconds
});
```

## Sync Database

You can force a sync to your remote database by calling the `sync()` method. This is only available for `libsql` databases:

```tsx
remoteDb.sync();
```

## Unsupported Features

However, by using `libsql` the following features are not available due to lack of support in the library itself:

- Running multiple statements in a single string
- Update/commit/rollback hooks
- Reactive queries
- Extension loading
- Local disk-encryption is currently not supported in libsql.
- Custom tokenizers
