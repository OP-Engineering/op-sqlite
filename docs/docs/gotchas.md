---
sidebar_position: 3
---

# Gotchas

SQLite being a C library and React Native being a JS framework with native parts some times create conflicts. Here are the most common problems.

## JavaScript and Numbers

JavaScript represents every number internally as a `double`. This means you can only have integers represented up to 2^53 bits(`Number.MAX_SAFE_INTEGER`). Although sqlite supports long long (2^64 bits) the numbers will be truncated when you query a value bigger than what a JS number can represent. If you need to store larger numbers you should use a `bigint`, however, such a type is not natively supported by sqlite, so you will have to serialize and deserialize from/to bigint when you do your queries:

```tsx
// When inserting, convert bigint into a string
await db.execute('INSERT INTO NumbersTable VALUES (?)', [
  bigint('123').toString(),
]);

// When retrieving, convert string into bigint
let res = await db.execute('SELECT * FROM NumbersTable');
let myBigint = BigInt(res.rows[0].number);
```

## SQLite Gotchas

### Strictness

SQLite by default does not strictly check for types. if you want true type safety when you declare your tables you need to use the `STRICT` keyword.

```tsx
await db.execute('CREATE TABLE Test (id INT PRIMARY KEY, name TEXT) STRICT;');
```

If you don't set it, SQLite will happily write whatever you insert in your table, independetly of the declared type (it will try to cast it though, e.g. a `"1"` string might be turned to a `1` int).

### Foreign constraints

When SQLite evaluates your query and you have forgein key constraints, it keeps track of the satisfied relations via a counter. Once your statement finishes executing and the counter is not 0, it throws a foreign key constraint failed error. Unfortunately, this simple design means it is impossible to catch which foreign constraint is failed and you will receive a generic error. Nothing op-sqlite can do about it, it's a design flaw in SQLite.

In order to catch foreign key errors, you also need to execute the pragma when you open your connection:

```tsx
await db.execute('PRAGMA foreign_keys = true');
```

### Error codes

Sometimes you might be using valid SQL syntax for other engines or you might be doing something else wrong. The errors returned by op-sqlite contain the raw error code returned by SQLite and you should check [the reference](https://www.sqlite.org/rescode.html) for more detailed information.

### Other Quirks

See the [full list of SQLite quirks](https://www.sqlite.org/quirks.html).

## HostObjects Quirks

op-sqlite can return HostObjects via the `executeWithHostObjects` API, basically C++ instances exposed to the JS context. They are super fast to create at the cost of runtime access. However, this means some operations won't work.

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
