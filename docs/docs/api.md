---
sidebar_position: 9
---

# API Reference

## Open

You start by opening a connection to your DB. It’s recommended you only open one connection **per App session**. There is no need to open and close it, as a matter of fact, it will add a lot of latency to your queries.

```jsx
import { open } from '@op-engineering/op-sqlite';

export const db = open({
  name: 'myDb.sqlite',
});
```

### SQLCipher Open

If you are using SQLCipher all the methods are the same with the exception of the open method which needs an extra `encryptionKey` to encrypt/decrypt the database.

```tsx
import { open } from '@op-engineering/op-sqlite';

export const db = open({
  name: 'myDb.sqlite',
  encryptionKey: 'YOUR ENCRYPTION KEY, KEEP IT SOMEWHERE SAFE', // for example op-s2
});
```

If you want to read more about securely storing your encryption key, [read this article](https://ospfranco.com/react-native-security-guide/). Again: **DO NOT OPEN MORE THAN ONE CONNECTION PER DATABASE**. Just export one single db connection for your entire application and reuse it everywhere.

## Execute

Base async query operation. All execute calls run on a (**single**) separate and dedicated thread, so the JS thread is not blocked. It’s recommended to ALWAYS use transactions since even read calls can corrupt a sqlite database.

```tsx
import { open } from '@op-engineering/op-sqlite';

try {
  const db = open({ name: 'myDb.sqlite' });

  let { rows } = await db.execute('SELECT somevalue FROM sometable');

  rows.forEach((row) => {
    console.log(row);
  });
} catch (e) {
  console.error('Something went wrong executing SQL commands:', e.message);
}
```

### Execute with Host Objects

It’s possible to return HostObjects when using a query. The benefit is that HostObjects are only created in C++ and only when you try to access a value inside of them a C++ value → JS value conversion happens. This means creation is fast, property access is slow. The use case is clear if you are returning **massive** amount of objects but only displaying/accessing a few of them at the time.

The example of querying 300k objects from a database uses this api. Just be careful with all the gotchas of HostObjects (no spread, no logging, etc.)

```tsx
let res = await db.executeWithHostObjects('select * from USERS');
```

## Prepared statements

A lot of the work when executing queries is not iterating through the result set itself but, sometimes, planning the execution. If you have a query which is expensive but you can reuse it (even if you have to change the arguments) you can use a `prepared statement`. Bear in mind most of the benefit of a prepared statement is in the querying, joining and planning on how to retrieve the data, for writes the impact is minimal.

```tsx
const statement = db.prepareStatement('SELECT * FROM User WHERE name = ?;');

// bind the variables in the order they appear
await statement.bind(['Oscar']);
// Or use the bindsync version
statement.bindSync(['Luis']);
let results1 = await statement.execute();

await statement.bind(['Carlos']);
let results2 = await statement.execute();
```

You only pay the price of parsing the query once, and each subsequent execution should be faster.

## Raw execution

If you don't care about the keys you can use a simplified execution that will return an array of scalars. This should be a lot faster than the regular operation since objects with the same keys don’t need to be created.

```tsx
let result = await db.executeRaw('SELECT * FROM Users;');
// result = [[123, 'Katie', ...]]
```

### Multiple Statements

You can execute multiple statements in a single operation. The API however is not really thought for this use case and the results (and their metadata) will be mangled, so you can discard it. This is not supported in libsql, due to the library itself not supporting this use case.

```tsx
// The result of this query will all be all mixed, no point in trying to read it
let res = await db.execute(
  `CREATE TABLE T1 (id INT PRIMARY KEY) STRICT;
  CREATE TABLE T2 (id INT PRIMARY KEY) STRICT;`
);
```

### Sync execute

You can do sync queries via the `executeSync` functions. Not available in transactions and must be used sparingly as it blocks the UI thread.

```jsx
let res = db.executeSync('SELECT 1');
```

## Transactions

Wraps the code inside in a transaction. Any error thrown inside of the transaction body function will ROLLBACK the transaction.

If you want to execute a large set of commands as fast as possible you should use the `executeBatch` method, it wraps all the commands in a transaction for you and has less overhead. Using prepared statements for writes inside a transaction is discouraged, you gain very little performance when writing to the database and they are not part of the internal lock mechanism of op-sqlite.

```tsx
await db.transaction((tx) => {
  const { status } = await tx.execute(
    'UPDATE sometable SET somecolumn = ? where somekey = ?',
    [0, 1]
  );

  // Any uncatched error ROLLBACK transaction
  throw new Error('Random Error!');

  // You can manually commit or rollback
  await tx.commit();
  // or
  await tx.rollback();
});
```

## Batch Execution

Allows to execute a batch of commands in a single call. The entire call is wrapped within a transaction, so if any of the statements fail, they all get rolled back.

```tsx
const commands = [
  ['CREATE TABLE TEST (id integer)'],
  ['INSERT INTO TEST (id) VALUES (?)', [1]],
  [('INSERT INTO TEST (id) VALUES (?)', [2])],
  [('INSERT INTO TEST (id) VALUES (?)', [[3], [4], [5], [6]])],
];

const res = await db.executeBatch(commands);

console.log(`Batch affected ${result.rowsAffected} rows`);
```

In some scenarios, dynamic applications may need to get some metadata information about the returned result set.

## Blob support

Blobs are supported via `ArrayBuffer` or typed array (UInt8Array, UInt16Array, etc) directly. Here is an example:

```tsx
db = open({
  name: 'blobs',
});

await db.execute('DROP TABLE IF EXISTS BlobTable;');
await db.execute(
  'CREATE TABLE BlobTable ( id INT PRIMARY KEY, name TEXT NOT NULL, content BLOB) STRICT;'
);

let binaryData = new Uint8Array(2);
binaryData[0] = 42;

await db.execute(`INSERT OR REPLACE INTO BlobTable VALUES (?, ?, ?);`, [
  1,
  'myTestBlob',
  binaryData,
]);

const result = await db.execute('SELECT content FROM BlobTable');

const finalUint8 = new Uint8Array(result.rows[0].content);
```

# Attach or Detach other databases

SQLite supports attaching or detaching other database files into your main database connection through an alias. You can do any operation you like on this attached database like JOIN results across tables in different schemas, or update data or objects. These databases can have different configurations, like journal modes, and cache settings.

You can, at any moment, detach a database that you don't need anymore. You don't need to detach an attached database before closing your connection. Closing the main connection will detach any attached databases.

SQLite has a limit for attached databases: A default of 10, and a global max of 125

SQLite docs for [Attach](https://www.sqlite.org/lang_attach.html) - [Detach](https://www.sqlite.org/lang_detach.html)

```tsx
// Follows similar API to the `open` call
db.attach({
  secondaryDbFileName: 'statistics.sqlite', // Filename of the database (JUST THE FILENAME)
  alias: 'stats', // Alias to be applied to the db
  location: '../databases', // Path to be prepended to secondaryFileName, in this case full db path: ../databases/statistics.sqlite
});

const res = await db.execute(
  'SELECT * FROM some_table_from_mainschema a INNER JOIN stats.some_table b on a.id_column = b.id_column'
);

// You can detach databases at any moment
db.detach('stats');
```

## Loading SQL Dump Files

If you are trying to load a large set of statements (the larger the more benefit you will see, talking about hundreds of thousands of rows here) or restoring a database backup the fastest way possible will be loading a sqlite dump file. it will be the fastest as there is no back and forth between JS and C++ and SQLite doing all the heavy lifting.

```tsx
const { rowsAffected, commands } = await db.loadFile(
  '/absolute/path/to/file.sql'
);
```

## Hooks

You can subscribe to changes in your database by using an update hook:

```tsx
// Bear in mind: rowId is not your table primary key but the internal rowId sqlite uses
// to keep track of the table rows
db.updateHook(({ rowId, table, operation }) => {
  console.warn(`Hook has been called, rowId: ${rowId}, ${table}, ${operation}`);
  const changes = await db.execute('SELECT * FROM User WHERE rowid = ?', [
    rowid,
  ]);
});

await db.execute(
  'INSERT INTO User (id, name, age, networth) VALUES(?, ?, ?, ?)',
  [id, name, age, networth]
);
```

Same goes for commit and rollback hooks

```tsx
// will fire whenever a transaction commits
db.commitHook(() => {
  console.log('Transaction committed!');
});

db.rollbackHook(() => {
  console.log('Transaction rolled back!');
});

// will fire the commit hook
db.transaction(async (tx) => {
  tx.execute(
    'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
    [id, name, age, networth]
  );
});

// will fire the rollback hook
try {
  await db.transaction(async (tx) => {
    throw new Error('Test Error');
  });
} catch (e) {
  // intentionally left blank
}
```

You can pass `null`` to remove hooks at any moment:

```tsx
db.updateHook(null);

db.commitHook(null);

db.rollbackHook(null);
```

## Database Path

Allows to get the file location on disk. Useful for debugging or attaching the file to bug tickets.

```tsx
const path = db.getDbPath();
```

## Move assets database

Allows to easily move a database from your app bundled assets to the documents folder. You NEED to move databases before using them. The bundle gets replaced every time an app update happens, so changes will get lost otherwise. On Android the app bundle is zipped so you cannot interact with any database on the bundle. If the database has already been created the operation does nothing. Double check you have not created the database before or use `overwrite` to overwrite the existing database on disk.

```tsx
const copied = await moveAssetsDatabase({
  filename: 'sample2.sqlite',
  path: 'sqlite', // The path inside your assets folder on Android, on iOS the file structure is flat
  overwrite: true, // Always overwrite the database
});

expect(copied).to.equal(true);
```

## JSONB Support

Sqlite comes with JSONB support included, no need to load any extension or compilation flag. You should read up on how [Sqlite JSONB works](https://fedoramagazine.org/json-and-jsonb-support-in-sqlite-3-45-0/). op-sqlite is an (almost) direct binding to sqlite so you need to use the responses as if they would come from sqlite itself. You can insert a string, use JSONB functions and get a string response:

```jsx
await db.execute('CREATE TABLE states(data TEXT);');
await db.execute(
  `INSERT INTO states VALUES ('{"country":"Luxembourg","capital":"Luxembourg City","languages":["French","German","Luxembourgish"]}');`
);
let res = await db.execute(
  `SELECT data->>'country' FROM states WHERE data->>'capital'=='Amsterdam';`
);
let res2 = await db.execute(
  `SELECT jsonb_extract(data, '$.languages') FROM states;`
);
```

As mentioned in the article you can also use blobs, in either case you need to convert your own data to a string or an ArrayBuffer, this is not done for you.

```jsx
let states = // some JS object
  await db.execute('INSERT INTO states VALUES (?)', [JSON.stringify(states)]);
// or if using blobs

function objectToArrayBuffer(obj) {
  // Step 1: Serialize the object to a JSON string
  const jsonString = JSON.stringify(obj);

  // Step 2: Encode the string to UTF-8
  const encoder = new TextEncoder();
  const uint8Array = encoder.encode(jsonString);

  // Step 3: Convert Uint8Array to ArrayBuffer
  return uint8Array.buffer;
}

await db.execute('INSERT INTO states VALUES (?)', [
  objectToArrayBuffer(states),
]);
```

## Loading Extensions

Loading runtime extensions is supported. You need compile your extension to the correct architecture. Each extension has different build process, so you need to figure how to compile it yourself.

### Android

- Compile your extension to a dynamic library (.so)
- Compile for each common Android architecture, then place them in a folder `[PROJECT ROOT]/android/app/src/main/jniLibs`. That path is special and will automatically be packaged inside your app. It will look something like this:

  ```text
  /android
    /app
      /src
        /main
          /jniLibs
            /arm64-v8a
              libcrsqlite.so
            /armeabi-v7a
              libcrsqlite.so
            /x86
              libcrsqlite.so
            /x86_64
              libcrsqlite.so
  ```

### iOS

- Follow the [Guide to generating iOS dynamic libraries](https://ospfranco.com/generating-a-xcframework-with-dylibs-for-ios/). The process is far more complex, so make sure you follow the steps in detail and create a correct `.xcframework`
- Unlike Android, iOS does not load the dylib for you automatically, you need to first call `getDylibPath` to get the runtime path of the dylib you created.

### Loading the extension

- On android `getDylibPath` is a no-op and you just need to pass the canonical name of the library. You can then finally call the `loadExtension` function on your database:

  ```tsx
  import {open, getDylibPath} from '@op-sqlite/op-engineering';

  const db = open(...);
  let path = "libcrsqlite" // in Android it will be the name of the .so
  if (Platform.os == "ios") {
    path = getDylibPath("io.vlcn.crsqlite", "crsqlite"); // You need to get the bundle name from the .framework/plist.info inside of the .xcframework you created and then the canonical name inside the same plist
  }
  // Extensions usually have a default entry point to be loaded, if the documentation says nothing, you should assume no entry point change
  db.loadExtension(path);

  // Others might need a different entry point function, you can pass it as a second argument
  db.loadExtension(path, "entry_function_of_the_extension");
  ```

## Reactive Queries

Reactive queries have their own section in the docs.
