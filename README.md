![screenshot](https://raw.githubusercontent.com/OP-Engineering/op-sqlite/main/Header.png)

<div align="center">
  <pre align="center">
    yarn add @op-engineering/op-sqlite
    npx pod-install</pre>
  <br />
</div>
<br />

OP SQLite embeds the latest version of SQLite and provides a low-level API to execute SQL queries.

**Current SQLite version: 3.44.0**

Created by [@ospfranco](https://twitter.com/ospfranco). **Please consider Sponsoring**, none of this work is for free. I pay for it with my time and knowledge. If you are a company in need of help with your React Native/React apps feel free to reach out. I also do a lot of C++ and nowadays Rust. 

## Benchmarks

You can find the [benchmarking code in the example app](https://github.com/OP-Engineering/op-sqlite/blob/main/example/src/Database.ts#L44). Non JSI libraries are not even a contender anymore, you should expect anywhere between a 5x to a 8x improvement over sqlite-storage, sqlite2 and so on. Loading a 300k record database (in milliseconds).

| Library      | iPhone 15 Pro | Galaxy S22 |
| ------------ | ------------- | ---------- |
| quick-sqlite | 2719ms        | 8851ms     |
| expo-sqlite  | 2293ms        | 10626ms    |
| op-sqlite    | 507ms         | 1125ms     |

Memory consumption is also is also 1/4 compared to `react-native-quick-sqlite`. This query used to take 1.2gb of peak memory usage, now runs in 250mbs.

# Encryption

If you need to encrypt your entire database, there is [`op-sqlcipher`](https://github.com/OP-Engineering/op-sqlcipher), which is a fork of this library which uses [SQLCipher](https://github.com/sqlcipher/sqlcipher). It completely encrypts all the database with minimal overhead. Bear in mind, however, it is a fork maintained by a third-party.

# DB Paths

The library creates/opens databases by appending the passed name plus, the [library directory on iOS](https://github.com/OP-Engineering/op-sqlite/blob/main/ios/OPSQLite.mm#L51) and the [database directory on Android](https://github.com/OP-Engineering/op-sqlite/blob/main/android/src/main/java/com/op/sqlite/OPSQLiteBridge.java#L18). If you are migrating from `react-native-quick-sqlite` you will have to move your library using one of the many react-native fs libraries.

If you have an existing database file you want to load you can navigate from these directories using dot notation. e.g.:

```ts
import { open } from '@op-engineering/op-sqlite';

const largeDb = open({
  name: 'largeDB',
  location: '../files/databases',
});
```

Note that on iOS the file system is sand-boxed, so you cannot access files/directories outside your app bundle directories.

## In memory

Using SQLite in memory mode is supported:

```ts
import { open } from '@op-engineering/op-sqlite';

const largeDb = open({
  name: 'inMemoryDb',
  inMemory: true,
});
```

# API

```typescript
import {open} from '@op-engineering/op-sqlite'

const db = open({name: 'myDb.sqlite'})

// The db object contains the following methods:
db = {
  close: () => void,
  delete: () => void,
  attach: (dbNameToAttach: string, alias: string, location?: string) => void,
  detach: (alias: string) => void,
  transaction: (fn: (tx: Transaction) => void) => Promise<void>,
  execute: (query: string, params?: any[]) => QueryResult,
  executeAsync: (
    query: string,
    params?: any[]
  ) => Promise<QueryResult>,
  executeBatch: (commands: SQLBatchParams[]) => BatchQueryResult,
  executeBatchAsync: (commands: SQLBatchParams[]) => Promise<BatchQueryResult>,
  loadFile: (location: string) => Promise<FileLoadResult>
}
```

### Simple queries

The basic query is **synchronous**, it will block rendering on large operations, further below you will find async versions.

```ts
import { open } from '@op-engineering/op-sqlite';

try {
  const db = open({ name: 'myDb.sqlite' });

  let { rows } = db.execute('SELECT somevalue FROM sometable');

  // _array internally holds the values, this is meant to comply with the webSQL spec
  rows._array.forEach((row) => {
    console.log(row);
  });

  let { rowsAffected } = await db.executeAsync(
    'UPDATE sometable SET somecolumn = ? where somekey = ?',
    [0, 1]
  );

  console.log(`Update affected ${rowsAffected} rows`);
} catch (e) {
  console.error('Something went wrong executing SQL commands:', e.message);
}
```

### Multiple statements in a single string

You can execute multiple statements in a single operation. The API however is not really thought for this use case and the results (and their metadata) will be mangled, so you can discard it.

```ts
// The result of this query will all be in a single array, no point in trying to read it
db.execute(
  `CREATE TABLE T1 ( id INT PRIMARY KEY) STRICT;
  CREATE TABLE T2 ( id INT PRIMARY KEY) STRICT;`
);

let t1name = db.execute(
  "SELECT name FROM sqlite_master WHERE type='table' AND name='T1';"
);

console.log(t1name.rows?._array[0].name); // outputs "T1"

let t2name = db.execute(
  "SELECT name FROM sqlite_master WHERE type='table' AND name='T2';"
);

console.log(t2name.rows?._array[0].name); // outputs "T2"
```

### Transactions

Throwing an error inside the callback will ROLLBACK the transaction.

If you want to execute a large set of commands as fast as possible you should use the `executeBatch` method, it wraps all the commands in a transaction and has less overhead.

```typescript
await db.transaction('myDatabase', (tx) => {
  const { status } = tx.execute(
    'UPDATE sometable SET somecolumn = ? where somekey = ?',
    [0, 1]
  );

  // offload from JS thread
  await tx.executeAsync = tx.executeAsync(
    'UPDATE sometable SET somecolumn = ? where somekey = ?',
    [0, 1]
  );

  // Any uncatched error ROLLBACK transaction
  throw new Error('Random Error!');

  // You can manually commit or rollback
  tx.commit();
  // or
  tx.rollback();
});
```

### Batch operation

Batch execution allows the transactional execution of a set of commands

```typescript
const commands = [
  ['CREATE TABLE TEST (id integer)'],
  ['INSERT INTO TEST (id) VALUES (?)', [1]],
  [('INSERT INTO TEST (id) VALUES (?)', [2])],
  [('INSERT INTO TEST (id) VALUES (?)', [[3], [4], [5], [6]])],
];

const res = db.executeSqlBatch('myDatabase', commands);

console.log(`Batch affected ${result.rowsAffected} rows`);
```

### Dynamic Column Metadata

In some scenarios, dynamic applications may need to get some metadata information about the returned result set.

This can be done by testing the returned data directly, but in some cases may not be enough, for example when data is stored outside
SQLite datatypes. When fetching data directly from tables or views linked to table columns, SQLite can identify the table declared types:

```typescript
let { metadata } = db.executeSql(
  'myDatabase',
  'SELECT int_column_1, bol_column_2 FROM sometable'
);

metadata.forEach((column) => {
  // Output:
  // int_column_1 - INTEGER
  // bol_column_2 - BOOLEAN
  console.log(`${column.name} - ${column.type}`);
});
```

### Async operations

You might have too much SQL to process and it will cause your application to freeze. There are async versions for some of the operations. This will offload the SQLite processing to a different thread.

```ts
db.executeAsync(
  'myDatabase',
  'SELECT * FROM "User";',
  []).then(({rows}) => {
    console.log('users', rows._array);
  })
);
```

### Blobs

Blobs are supported via `ArrayBuffer`, you need to be careful about the semantics though. You cannot instanciate an instance of `ArrayBuffer` directly, nor pass a typed array directly. Here is an example:

```ts
db = open({
  name: 'blobs',
});

db.execute('DROP TABLE IF EXISTS BlobTable;');
db.execute(
  'CREATE TABLE BlobTable ( id INT PRIMARY KEY, name TEXT NOT NULL, content BLOB) STRICT;'
);

let binaryData = new Uint8Array(2);
binaryData[0] = 42;

db.execute(`INSERT OR REPLACE INTO BlobTable VALUES (?, ?, ?);`, [
  1,
  'myTestBlob',
  binaryData,
]);

const result = db.execute('SELECT content FROM BlobTable');

const finalUint8 = new Uint8Array(result.rows!._array[0].content);
```

### Attach or Detach other databases

SQLite supports attaching or detaching other database files into your main database connection through an alias.
You can do any operation you like on this attached database like JOIN results across tables in different schemas, or update data or objects.
These databases can have different configurations, like journal modes, and cache settings.

You can, at any moment, detach a database that you don't need anymore. You don't need to detach an attached database before closing your connection. Closing the main connection will detach any attached databases.

SQLite has a limit for attached databases: A default of 10, and a global max of 125

References: [Attach](https://www.sqlite.org/lang_attach.html) - [Detach](https://www.sqlite.org/lang_detach.html)

```ts
db.attach('mainDatabase', 'statistics', 'stats', '../databases');

const res = db.executeSql(
  'mainDatabase',
  'SELECT * FROM some_table_from_mainschema a INNER JOIN stats.some_table b on a.id_column = b.id_column'
);

// You can detach databases at any moment
db.detach('mainDatabase', 'stats');
if (!detachResult.status) {
  // Database de-attached
}
```

### Loading SQL Dump Files

If you have a SQL dump file, you can load it directly, with low memory consumption:

```typescript
const { rowsAffected, commands } = db
  .loadFile('myDatabase', '/absolute/path/to/file.sql')
  .then((res) => {
    const { rowsAffected, commands } = res;
  });
```

## Use built-in SQLite

On iOS you can use the embedded SQLite, when running `pod-install` add an environment flag:

```
OP_SQLITE_USE_PHONE_VERSION=1 npx pod-install
```

On Android, it is not possible to link the OS SQLite. It is also a bad idea due to vendor changes, old android bugs, etc. Unfortunately, this means this library will add some megabytes to your app size.

## Enable compile-time options

By specifying pre-processor flags, you can enable optional features like FTS5, Geopoly, etc.

### iOS

Add a `post_install` block to your `<PROJECT_ROOT>/ios/Podfile` like so:

```ruby
post_install do |installer|
  installer.pods_project.targets.each do |target|
    if target.name == "op-sqlite" then
      target.build_configurations.each do |config|
        config.build_settings['GCC_PREPROCESSOR_DEFINITIONS'] << 'SQLITE_ENABLE_FTS5=1'
      end
    end
  end
end
```

Replace the `<SQLITE_FLAGS>` part with the flags you want to add.
For example, you could add `SQLITE_ENABLE_FTS5=1` to `GCC_PREPROCESSOR_DEFINITIONS` to enable FTS5 in the iOS project.

### Android

You can specify flags via `<PROJECT_ROOT>/android/gradle.properties` like so:

```
OPSQLiteFlags="-DSQLITE_ENABLE_FTS5=1"
```

## Additional configuration

### App groups (iOS only)

On iOS, the SQLite database can be placed in an app group, in order to make it accessible from other apps in that app group. E.g. for sharing capabilities.

To use an app group, add the app group ID as the value for the `OPSQLite_AppGroup` key in your project's `Info.plist` file. You'll also need to configure the app group in your project settings. (Xcode -> Project Settings -> Signing & Capabilities -> Add Capability -> App Groups)

## License

MIT License.
