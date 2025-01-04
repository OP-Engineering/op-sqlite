---
sidebar_position: 2
---

# Configuration

## Database Location

### Default location

If you don't pass a `location` the library creates/opens databases by appending the passed name plus, the [library directory on iOS](https://github.com/OP-Engineering/op-sqlite/blob/main/ios/OPSQLite.mm#L51) and the [database directory on Android](https://github.com/OP-Engineering/op-sqlite/blob/main/android/src/main/java/com/op/sqlite/OPSQLiteBridge.java#L18).

### Relative location

You can use relative location to navigate in and out of the default location

```tsx
import { open } from '@op-engineering/op-sqlite';

const db = open({
  name: 'myDB',
  location: '../files/databases',
});
```

Note that on iOS the file system is sand-boxed, so you cannot access files/directories outside your app bundle directories.

### Absolute paths

You can also pass absolute paths to completely change the location of the database, the library exports useful paths you can use:

```tsx
import {
  IOS_LIBRARY_PATH, // Default iOS
  IOS_DOCUMENT_PATH,
  ANDROID_DATABASE_PATH, // Default Android
  ANDROID_FILES_PATH,
  ANDROID_EXTERNAL_FILES_PATH, // Android SD Card
  open,
} from '@op-engineering/op-sqlite';

const db = open({
  name: 'myDb',
  location: Platform.OS === 'ios' ? IOS_LIBRARY_PATH : ANDROID_DATABASE_PATH,
});
```

If you want to access the SD card app's directory:

```tsx
const db = open({
  name: 'myDB',
  location:
    Platform.OS === 'ios' ? IOS_LIBRARY_PATH : ANDROID_EXTERNAL_FILES_PATH,
});
```

You can even drill down on paths:

```tsx
const db = open({
  name: 'myDB',
  location:
    Platform.OS === 'ios'
      ? IOS_LIBRARY_PATH
      : `${ANDROID_EXTERNAL_FILES_PATH}/dbs/`,
});
```

### In-memory

Using SQLite in-memory mode is supported by passing a `':memory:'` as a location:

```tsx
import { open } from '@op-engineering/op-sqlite';

const largeDb = open({
  name: 'inMemoryDb',
  location: ':memory:',
});
```

In memory databases are faster since they don't need to hit the disk I/O to save the data and are useful for synchronization only workflows.

## Loading an existing database

Depending on where you have your database there are a few things to go about this:

- If you add it to your bundle (via assets): YOU MUST MOVE IT BEFORE WRITING TO IT. App assets are read only anyways. In order to make the moving process easier OP SQLite provides a function that easily moves it for you!

  - Create an assets folder
  - Place your database at the root of that folder, in this example we will place a `assets/sample.sqlite`
  - Configure `react-native.config.js` correctly
    ```jsx
    module.exports = {
      assets: ['./assets/'],
    };
    ```
  - Link your assets
    ```bash
    npx react-native-asset@latest
    ```
  - Finally when your app starts you can move it to the default location an open it normally. The `moveAssetsDatabase` is idempotent, if the database has already been copied it will not do anything but return true, so it’s safe to leave there.

    ```tsx
    import { moveAssetsDatabase, open } from '@op-engineering/op-sqlite';

    const openAssetsDb = async () => {
      const moved = await moveAssetsDatabase({ filename: 'sample.sqlite' });
      if (!moved) {
        throw new Error('Could not move assets database');
      }
      const db = open({ name: 'sample.sqlite' });
      const users = await db.execute('SELECT * FROM User');
      console.log('users', users.rows);
    };
    ```

- If you have a server you can download it directly to the default directory and directly open it.

  ```tsx
  import FetchBlob from 'react-native-fetch-blob';
  import {
    IOS_LIBRARY_PATH, // Default iOS
    ANDROID_DATABASE_PATH, // Default Android
  } from '@op-engineering/op-sqlite';

  // Pseudo-code replace with the proper calls however you download the database
  async function downloadAndMove() {
    await FetchBlob.download(
      `<download url>/sample.sqlite`,
      Platform.OS === 'ios' ? IOS_LIBRARY_PATH : ANDROID_DATABASE_PATH
    );
  }

  openDb = () => {
    const db = open({ name: 'sample.sqlite' });
    const users = await db.execute('SELECT * FROM User');
    console.log('users', users.rows?._array);
  };
  ```

# App groups (iOS only)\*\*

On iOS, the SQLite database can be placed in an app group, in order to make it accessible from other apps in that app group. E.g. for sharing capabilities between apps, widgets or quick-apps.

To use an app group, add the app group ID as the value for the `OPSQLite_AppGroup` key in your project's `Info.plist` file. You'll also need to configure the app group in your project settings. (Xcode → Project Settings → Signing & Capabilities → Add Capability → App Groups)

# Runtime tweaks

You can tweak SQLite to be even faster (with some caveats) on runtime as well. One option is [Memory Mapping](https://www.sqlite.org/mmap.html). It allows to read/write to/from the disk without going through the kernel. However, if your queries throw an error your application might crash.

To turn on Memory Mapping, execute the following pragma statement after opening a db:

```tsx
const db = open({
  name: 'mydb.sqlite',
});

// 0 turns off memory mapping, any other number enables it with the cache size
await db.execute('PRAGMA mmap_size=268435456');
```

You can also set journaling to memory (or even OFF if you are kinda crazy) to gain even more speed. Journaling is what allows SQLite to ROLLBACK statements and modifying it dangerous, so do it at your own risk

```tsx
await db.execute('PRAGMA journal_mode = MEMORY;'); // or OFF
```
