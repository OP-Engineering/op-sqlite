---
sidebar_position: 8
---

# Key-Value Storage

OP-SQLite provides a simple key-value storage API compatible with react-native-async-storage. Mostly as a convenience, use at your own caution. It uses sqlite as a simple text storage, so it might be slower than using other key-value packages or writing data to disk. If you use SQLCipher the data inside will also be encrypted.

```ts
import { Storage } from '@op-engineering/op-sqlite';

// Storage is backed by it's own database
// You can set the location like any other op-sqlite database
const storage = new Storage({
  location: 'storage', // Optional, see location param on normal databases
  encryptionKey: 'myEncryptionKey', // Optional, only used when used against SQLCipher
});

const item = storage.getItemSync('foo');

const item2 = await storage.getItem('foo');

await storage.setItem('foo', 'bar');

storage.setItemSync('foo', 'bar');

const allKeys = storage.getAllKeys();

// Clears the internal table
storage.clear();
```
