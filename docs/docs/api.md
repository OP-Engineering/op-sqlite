---
sidebar_position: 8
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

If you want to read more about securely storing your encryption key, [read this article](https://ospfranco.com/react-native-security-guide/). Again: **DO NOT OPEN MORE THAN ONE CONNECTION PER DATABASE**. Just export one single db connection for your entire application and re-use it everywhere.

## Execute

Base query operation. In runs in a separate so be careful with race conditions. Execution runs on native C++ threads always, so the JS/UI is not blocked, therefore, it’s recommended to ALWAYS use transactions.

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

It’s possible to return HostObjects when using a query. The benefit is that HostObjects are only created in C++ and only when you try to access a value inside of them a C++ scalar → JS scalar conversion happens. This means creation is fast, property access is slow. The use case is clear if you are returning **massive** amount of objects but only displaying/accessing a few of them at the time.

The example of querying 300k objects from a database uses this api. Just be careful with all the gotchas of HostObjects (no spread, no logging, etc.)

```tsx
let res = await db.executeWithHostObjects('select * from USERS');
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

If you want to execute a large set of commands as fast as possible you should use the `executeBatch` method, it wraps all the commands in a transaction for you and has less overhead.

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

Batch execution allows the transactional execution of a set of commands

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

## Prepared statements

A lot of the work when executing queries is not iterating through the result set itself but, sometimes, planning the execution. If you have a query which is expensive but you can re-use it (even if you have to change the arguments) you can use a `prepared statement`:

```tsx
const statement = db.prepareStatement('SELECT * FROM User WHERE name = ?;');

// bind the variables in the order they appear
statement.bind(['Oscar']);
let results1 = await statement.execute();

statement.bind(['Carlos']);
let results2 = await statement.execute();
```

You only pay the price of parsing the query once, and each subsequent execution should be faster.

## Raw execution

If you don't care about the keys you can use a simplified execution that will return an array of scalars. This should be a lot faster than the regular operation since objects with the same keys don’t need to be created.

```tsx
let result = await db.executeRaw('SELECT * FROM Users;');
// result = [[123, 'Katie', ...]]
```

# Attach or Detach other databases

SQLite supports attaching or detaching other database files into your main database connection through an alias. You can do any operation you like on this attached database like JOIN results across tables in different schemas, or update data or objects. These databases can have different configurations, like journal modes, and cache settings.

You can, at any moment, detach a database that you don't need anymore. You don't need to detach an attached database before closing your connection. Closing the main connection will detach any attached databases.

SQLite has a limit for attached databases: A default of 10, and a global max of 125

SQLite docs for [Attach](https://www.sqlite.org/lang_attach.html) - [Detach](https://www.sqlite.org/lang_detach.html)

```tsx
db.attach('mainDatabase', 'statistics', 'stats', '../databases');

const res = await db.execute(
  'SELECT * FROM some_table_from_mainschema a INNER JOIN stats.some_table b on a.id_column = b.id_column'
);

// You can detach databases at any moment
db.detach('mainDatabase', 'stats');
if (!detachResult.status) {
  // Database de-attached
}
```

## Loading SQL Dump Files

If you have a SQL dump file, you can load it directly, with low memory consumption:

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
  console.log('Transaction commmitted!');
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

## Loading extensions

You can also load runtime extensions to an open database. First you need compile your extension to the correct architecture. Each extension has different build process.

Here is a step by step process.

- Compile your extension as runtime loadable extension (.so android, a dylib with no extension on iOS, packed as an `.xcframework` that contains multiple `.framework`).
- If you are using expo you need to pre-build your app or add your files into the build process.
- For android you will need to generate 4 versions, each for each common Android architecture, then place them in a folder `[PROJECT ROOT]/android/app/src/main/jniLibs`. That path is special and will automatically be packaged inside your app. It will look something like this:
  ```tsx
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
- For iOS you will need to compile the library as iOS dynamic library. The process is tedious an error prone. I’ve wrote [how to do it here](https://ospfranco.com/generating-a-xcframework-with-dylibs-for-ios/). Follow the instructions to the letter. Once you have your `.xcframework` you add it to the files of your Xcode project in the `Frameworks` folder. You should see it in the Frameworks, Libraries and Embedded Content section in the project general tab.
- On iOS you sqlite cannot load the dylib for you automatically, you need to first call `getDylibPath` to get the runtime path of the dylib you created. On android this function is a noop and you just need to pass the canonical name of the library. You can then finally call the `loadExtension` function on your database:

```tsx
import {open, getDylibPath} from '@op-sqlite/op-engineering';

const db = open(...);
let path = "libcrsqlite" // in Android it will be the name of the .so. On Android sqlite can load .so files for you directly as it is a linux system.
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