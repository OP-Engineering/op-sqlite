---
sidebar_position: 3
---

# Gotchas

SQLite being a C library and React Native being a JS framework, sometimes create conflicts. You should take these into account when using the library.

## JavaScript Numbers

Every JS number is a `double`. This means you can only have integers represented up to `2^53` bits (`Number.MAX_SAFE_INTEGER`). Although sqlite supports 64 bit ints (`long long`), the numbers returned to JS will be truncated when you query a value bigger than what a JS number can represent.

If you need to store larger numbers you should use a `BigInt` object, however, such a type is not natively supported by sqlite, so you will have to serialize and deserialize from/to strings when you do your queries:

```ts
// Create your table with the correct types AND USE STRICT TYPING
db.executeSync(
  'CREATE TABLE IF NOT EXISTS NumbersTable (myBigInt TEXT NOT NULL) STRICT'
);

// When inserting, convert bigint into a string
db.executeSync('INSERT INTO NumbersTable VALUES (?)', [
  bigint('123').toString(),
]);

// When retrieving, convert string into bigint
let res = db.executeSync('SELECT * FROM NumbersTable');
let myBigint = BigInt(res.rows[0].myBigInt);
```

## SQLite Gotchas

### Strictness

Sqlite by default does not strictly check for types. If you want type safety, you need to use the `STRICT` keyword.

```tsx
await db.execute('CREATE TABLE Test (id INT PRIMARY KEY, name TEXT) STRICT;');
```

Otherwise, sqlite does a weak attempt at casting and if it fails it inserts whatever you passed into your table, independetly of the declared type. e.g. If you try to insert a string `"1"` into a `INTEGER` column, it will be casted to an int before insertion and behave as a number (see above) when the table is queried.

[Read the sqlite docs on data types and their loose typing and how it can be the source of subtle bugs](https://sqlite.org/datatype3.html).

### Foreign constraints

When sqlite evaluates your query and you have forgein key constraints, it keeps track of the satisfied relations via a simple counter. Once your statement finishes executing and the counter is not 0, it then throws a `foreign key constraint failed` error, with no information whatsoever of which foreign constraint failed. Unfortunately, this simple design means it is impossible to catch which foreign constraint has failed and you will receive a generic error. Nothing op-sqlite can do about it, it's a design flaw in SQLite.

Additionaly, the default behavior is not to enforce foreign key constraints. You need to turn the constraint checking on, after opening your connection:

```tsx
db.executeSync('PRAGMA foreign_keys = true');
```

### Error codes

Sometimes you might be using valid SQL syntax for other engines or you might be doing something else wrong. The errors returned by op-sqlite contain the raw error code returned by SQLite and you should check [the sqlite error reference](https://www.sqlite.org/rescode.html) for more detailed information.

### Other Quirks

See the [full list of SQLite quirks](https://www.sqlite.org/quirks.html) that also apply to op-sqlite.

## HostObjects Quirks

op-sqlite can return HostObjects via the `executeWithHostObjects` API, basically C++ objects exposed to JS. They are super fast to create at the cost of runtime access. However, by them being C++ objects, it means some JS operations won't work.

You can write single properties with scalars, for example:

```tsx
let results = await db.executeWithHostObjects('SELECT * FROM USER;');
results._array[0].newProp = 'myNewProp';
```

As for trying to assign any object to a property, unfortunately, won't work.

```tsx
results._array[0].newProp = { foo: 'bar' };
```

On the C++ side properties need to be [stored](https://github.com/OP-Engineering/op-sqlite/blob/main/cpp/DumbHostObject.cpp?rgh-link-date=2024-03-09T07%3A31%3A31Z#L62) and [cast to C++ types](https://github.com/OP-Engineering/op-sqlite/blob/main/cpp/utils.cpp?rgh-link-date=2024-03-09T07%3A31%3A31Z#L64). Mostly to prevent race conditions and de-allocation between the JS Runtime and C++. Basically, not a bug, but rather a limitation of HostObjects.

You might want to try to create a completely new pure JS object to achieve this:

```tsx
let newUser = { ...{}, ...results._array[0], newProp: { foo: 'bar' } };
```

Sometimes `{...item, blah: 'foo'}` gets transpiled to `Object.assign(item, {blah: 'foo'}`, so that’s why you might need to use the quirky `...{}` at the beginning.

## Closing a connection

If you use `react-native-restart` or your app has a non-standard lifecycle and “reloads” itself at some point during runtime. It’s important that you call `db.close()` before to avoid crashes and memory leaks.
