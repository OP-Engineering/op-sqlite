---
sidebar_position: 13
---

# Windows Support

op-sqlite now supports Windows through React Native Windows, enabling you to use SQLite in your Windows desktop applications.

## Installation

### Prerequisites

Before installing op-sqlite for Windows, ensure you have:

1. Windows 10 SDK (10.0.19041.0 or higher)
2. Visual Studio 2022 with:
   - Desktop development with C++
   - Universal Windows Platform development
3. React Native Windows set up in your project

### Setup React Native Windows

If you haven't already added Windows support to your React Native project:

```bash
npx react-native-windows-init --overwrite
```

### Install op-sqlite

```bash
npm install @op-engineering/op-sqlite
# or
yarn add @op-engineering/op-sqlite
```

### Auto-linking

The module supports auto-linking for React Native Windows. After installation, run:

```bash
npx react-native autolink-windows
```

## Usage

The API remains the same across all platforms. Windows-specific paths are available as constants:

```typescript
import {
  open,
  WINDOWS_LOCAL_DATA_PATH,
  WINDOWS_TEMP_PATH,
  WINDOWS_ROAMING_DATA_PATH,
} from '@op-engineering/op-sqlite';

// Open a database in the local app data folder
const db = open({
  name: 'myapp.db',
  location: WINDOWS_LOCAL_DATA_PATH, // Uses Windows ApplicationData.LocalFolder
});

// Execute queries as usual
const users = await db.execute('SELECT * FROM users');
```

## Windows-Specific Features

### Path Constants

Windows provides three main storage locations:

- `WINDOWS_LOCAL_DATA_PATH`: Local application data folder (persists on device)
- `WINDOWS_TEMP_PATH`: Temporary folder (may be cleaned by the system)
- `WINDOWS_ROAMING_DATA_PATH`: Roaming folder (syncs across devices for the same user)

### File System Access

Windows apps run in a sandboxed environment. The module automatically handles proper file access within the app's designated folders.

### Moving Assets

To include a pre-populated database with your Windows app:

1. Add the database file to your app's Assets folder
2. Use the `moveAssetsDatabase` function to copy it to a writable location:

```typescript
import { moveAssetsDatabase, open, WINDOWS_LOCAL_DATA_PATH } from '@op-engineering/op-sqlite';

// Copy database from assets to local folder
const moved = moveAssetsDatabase('prepopulated.db');

if (moved) {
  const db = open({
    name: 'prepopulated.db',
    location: WINDOWS_LOCAL_DATA_PATH,
  });
}
```

## Building for Windows

### Development Build

```bash
npx react-native run-windows
```

### Release Build

```bash
npx react-native run-windows --release
```

### Architectures

The module supports all Windows architectures:
- x86 (32-bit)
- x64 (64-bit)
- ARM64

## Current Limitations

### Extensions Not Yet Available

The following extensions are not yet available for Windows but will be added in future releases:

- SQLCipher (encryption)
- cr-sqlite
- sqlite-vec

These require Windows-specific builds of the native libraries.

### Performance Mode

Performance optimizations are enabled by default in Release builds. The same SQLite compilation flags used on other platforms are applied on Windows.

## Troubleshooting

### Build Errors

If you encounter build errors:

1. Ensure all Windows SDK requirements are installed
2. Clean and rebuild:
   ```bash
   cd windows
   msbuild /t:Clean
   npx react-native run-windows
   ```

### Database Path Issues

Always use the provided path constants rather than hardcoding paths. Windows Store apps have restricted file system access.

### Module Not Found

If the module isn't found after installation:

1. Ensure auto-linking ran successfully
2. Check that the OPSQLite project is referenced in your app's solution
3. Rebuild the solution in Visual Studio

## Example

Here's a complete example for Windows:

```typescript
import React, { useEffect, useState } from 'react';
import { View, Text, Button } from 'react-native';
import { open, WINDOWS_LOCAL_DATA_PATH } from '@op-engineering/op-sqlite';

function App() {
  const [users, setUsers] = useState([]);

  useEffect(() => {
    initDatabase();
  }, []);

  const initDatabase = async () => {
    const db = open({
      name: 'test.db',
      location: WINDOWS_LOCAL_DATA_PATH,
    });

    await db.execute(`
      CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        email TEXT
      )
    `);

    await db.execute(
      'INSERT INTO users (name, email) VALUES (?, ?)',
      ['John Doe', 'john@example.com']
    );

    const result = await db.execute('SELECT * FROM users');
    setUsers(result.rows);
  };

  return (
    <View>
      <Text>Users in Database:</Text>
      {users.map(user => (
        <Text key={user.id}>{user.name} - {user.email}</Text>
      ))}
    </View>
  );
}

export default App;
```

## Contributing

Windows support is newly added. If you encounter issues or have suggestions:

1. Check existing issues on GitHub
2. Provide detailed error messages and logs
3. Include your Windows version and React Native Windows version

## Future Enhancements

Planned improvements for Windows support:

- [ ] SQLCipher encryption support
- [ ] cr-sqlite extension
- [ ] sqlite-vec extension
- [ ] Windows-specific performance optimizations
- [ ] Windows ARM64 native builds
- [ ] UWP and Win32 app support
