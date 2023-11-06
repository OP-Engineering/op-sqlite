![screenshot](https://raw.githubusercontent.com/OP-Engineering/op-sqlite/main/Header.jpg)

<div align="center">
  <pre align="center">
    yarn add @ospfranco/op-sqlite
    npx pod-install</pre>
  <br />
  <a align="center" href="https://twitter.com/ospfranco">
    <img src="https://img.shields.io/twitter/follow/ospfranco?label=By%20%40ospfranco&style=social" />
  </a>
</div>
<br />

OP SQLite embeds the latest version of SQLite and provides a low-level (JSI-backed) API to execute SQL queries.

**Current SQLite version: 3.44.0**

## Benchmarks

You can find the benchmarking code in the example app. Loading a 300k record database:

![benchmark](https://raw.githubusercontent.com/OP-Engineering/op-sqlite/main/benchmark.png)

## API

```typescript
import {open} from '@op-engineering/op-sqlite'

const db = open('myDb.sqlite')

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

```typescript
import { open } from 'op-sqlite';

try {
  const db = open('myDb.sqlite');

  let { rows } = db.execute('SELECT somevalue FROM sometable');

  rows.forEach((row) => {
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

### Transactions

Throwing an error inside the callback will ROLLBACK the transaction.

If you want to execute a large set of commands as fast as possible you should use the `executeBatch` method, it wraps all the commands in a transaction and has less overhead.

```typescript
await OPSQLite.transaction('myDatabase', (tx) => {
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

const res = OPSQLite.executeSqlBatch('myDatabase', commands);

console.log(`Batch affected ${result.rowsAffected} rows`);
```

### Dynamic Column Metadata

In some scenarios, dynamic applications may need to get some metadata information about the returned result set.

This can be done by testing the returned data directly, but in some cases may not be enough, for example when data is stored outside
SQLite datatypes. When fetching data directly from tables or views linked to table columns, SQLite can identify the table declared types:

```typescript
let { metadata } = OPSQLite.executeSql(
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
OPSQLite.executeAsync(
  'myDatabase',
  'SELECT * FROM "User";',
  []).then(({rows}) => {
    console.log('users', rows);
  })
);
```

### Attach or Detach other databases

SQLite supports attaching or detaching other database files into your main database connection through an alias.
You can do any operation you like on this attached database like JOIN results across tables in different schemas, or update data or objects.
These databases can have different configurations, like journal modes, and cache settings.

You can, at any moment, detach a database that you don't need anymore. You don't need to detach an attached database before closing your connection. Closing the main connection will detach any attached databases.

SQLite has a limit for attached databases: A default of 10, and a global max of 125

References: [Attach](https://www.sqlite.org/lang_attach.html) - [Detach](https://www.sqlite.org/lang_detach.html)

```ts
OPSQLite.attach('mainDatabase', 'statistics', 'stats', '../databases');

const res = OPSQLite.executeSql(
  'mainDatabase',
  'SELECT * FROM some_table_from_mainschema a INNER JOIN stats.some_table b on a.id_column = b.id_column'
);

// You can detach databases at any moment
OPSQLite.detach('mainDatabase', 'stats');
if (!detachResult.status) {
  // Database de-attached
}
```

### Loading SQL Dump Files

If you have a plain SQL file, you can load it directly, with low memory consumption.

```typescript
const { rowsAffected, commands } = OPSQLite.loadFile(
  'myDatabase',
  '/absolute/path/to/file.sql'
);
```

Or use the async version which will load the file in another native thread

```typescript
OPSQLite.loadFileAsync('myDatabase', '/absolute/path/to/file.sql').then(
  (res) => {
    const { rowsAffected, commands } = res;
  }
);
```

## Use built-in SQLite

On iOS you can use the embedded SQLite, when running `pod-install` add an environment flag:

```
OP_SQLITE_USE_PHONE_VERSION=1 npx pod-install
```

On Android, it is not possible to link the OS SQLite. It is also a bad idea due to vendor changes, old android bugs, etc. Unfortunately, this means this library will add some megabytes to your app size.

# Loading existing DBs

The library creates/opens databases by appending the passed name plus, the [library directory on iOS](https://github.com/OP-Engineering/op-sqlite/blob/733e876d98896f5efc80f989ae38120f16533a66/ios/OPSQLite.mm#L34-L35) and the [database directory on Android](https://github.com/OP-Engineering/op-sqlite/blob/main/android/src/main/java/com/op/sqlite/OPSQLiteBridge.java#L16). If you are migrating from `react-native-quick-sqlite` you will have to move your library using one of the many react-native fs libraries.

If you have an existing database file you want to load you can navigate from these directories using dot notation. e.g. `../www/myDb.sqlite`. Note that on iOS the file system is sand-boxed, so you cannot access files/directories outside your app bundle directories.

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
OPSQLiteFlags="<SQLITE_FLAGS>"
```

## Additional configuration

### App groups (iOS only)

On iOS, the SQLite database can be placed in an app group, in order to make it accessible from other apps in that app group. E.g. for sharing capabilities.

To use an app group, add the app group ID as the value for the `OPSQLite_AppGroup` key in your project's `Info.plist` file. You'll also need to configure the app group in your project settings. (Xcode -> Project Settings -> Signing & Capabilities -> Add Capability -> App Groups)

## License

MIT License.
