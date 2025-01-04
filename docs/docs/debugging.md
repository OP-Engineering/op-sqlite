---
sidebar_position: 4
---

# Debugging

You might want to directly look at the data inside your database or export it from a client device to provide support.

## Get DB Path

In order to support these workflows OP SQLite supports outputting the database file path.

```tsx
const db = open({ name: 'dbPath.sqlite' });
await db.execute('CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT)');
const path = db.getDbPath();
console.warn(path);
Clipboard.setString(path);
```

## iOS Simulator

If you are running on the simulator you can put the path on the clipboard. Go to your Terminal and directly open the database file in your sqlite explorer application

## On-Device

For on user support you can use this file path to attach it to an email or to you own ticketing system (as long as it supports file uploads).
