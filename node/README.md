# @op-engineering/op-sqlite-node

Node.js adapter for the `@op-engineering/op-sqlite` API using `better-sqlite3`.

This package provides the same TypeScript API as the React Native version but runs on Node.js, allowing you to share database logic between your React Native app and Node.js services.

## Usage

The API is identical to the React Native version, so you can share code between platforms:

```typescript
import { open } from '@op-engineering/op-sqlite-node';

// Open a database
const db = open({
  name: 'mydb.sqlite',
  location: './data'  // optional, defaults to current directory
});

// Execute queries synchronously
const result = db.executeSync('SELECT * FROM users WHERE id = ?', [1]);
console.log(result.rows);

// Or asynchronously
const asyncResult = await db.execute('SELECT * FROM users');
console.log(asyncResult.rows);

// Use transactions
await db.transaction(async (tx) => {
  await tx.execute('INSERT INTO users (name) VALUES (?)', ['John']);
  await tx.execute('INSERT INTO posts (user_id, title) VALUES (?, ?)', [1, 'Hello']);
});

// Execute batch operations
await db.executeBatch([
  ['CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT)'],
  ['INSERT INTO users (name) VALUES (?)', ['Alice']],
  ['INSERT INTO users (name) VALUES (?)', ['Bob']],
]);

// Close the database
db.close();
```

## API Differences from React Native

While the API surface is identical, there are some behavioral differences:

### Supported Features
- ✅ `open()` - Open database with name and location
- ✅ `openV2()` - Open database with full path
- ✅ `execute()` / `executeSync()` - Query execution
- ✅ `executeRaw()` / `executeRawSync()` - Raw array results
- ✅ `executeBatch()` - Batch operations in transaction
- ✅ `transaction()` - Transaction support
- ✅ `prepareStatement()` - Prepared statements
- ✅ `attach()` / `detach()` - Database attachment
- ✅ `loadFile()` - Load and execute SQL files
- ✅ `loadExtension()` - SQLite extensions (if enabled in better-sqlite3)
- ✅ `close()` / `delete()` - Database management

### Not Supported / Limited
- ❌ `openRemote()` - LibSQL remote connections (use libsql client directly)
- ❌ `openSync()` - LibSQL sync functionality (use libsql client directly)
- ❌ `sync()` - LibSQL sync method
- ❌ `reactiveExecute()` - Reactive queries (React Native specific)
- ❌ `updateHook()` - Update hooks (not supported by better-sqlite3)
- ❌ `setReservedBytes()` / `getReservedBytes()` - SQLCipher specific
- ⚠️ `encryptionKey` - Not supported (use @journeyapps/sqlcipher instead)
- ⚠️ `executeWithHostObjects()` - Falls back to regular execute
