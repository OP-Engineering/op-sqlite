# op-sqlite Windows Implementation

This directory contains the Windows implementation of op-sqlite using React Native Windows.

## Structure

```
windows/
├── OPSQLite/
│   ├── OPSQLiteModule.h        # Main module header
│   ├── OPSQLiteModule.cpp       # Module implementation with JSI bindings
│   ├── ReactPackageProvider.h   # Package provider header
│   ├── ReactPackageProvider.cpp # Package provider implementation
│   ├── pch.h                    # Precompiled header
│   ├── pch.cpp                  # Precompiled header source
│   ├── OPSQLite.vcxproj        # Visual Studio project file
│   ├── OPSQLite.def            # Module definition file
│   ├── PropertySheet.props      # Build properties
│   └── packages.config          # NuGet packages configuration
└── OPSQLite.sln                # Visual Studio solution file
```

## Implementation Details

### JSI Bindings

The Windows implementation uses the same JSI (JavaScript Interface) bindings as iOS and Android. The C++ core from the `cpp/` directory is shared across all platforms.

Key components:
- **OPSQLiteModule**: The main native module that installs JSI bindings
- **ReactPackageProvider**: Registers the module with React Native Windows
- **Path Handling**: Uses Windows ApplicationData APIs for proper sandboxed file access

### Storage Locations

Windows provides three storage locations:
- `WINDOWS_LOCAL_DATA_PATH`: Local app data (persists on device)
- `WINDOWS_TEMP_PATH`: Temporary folder (may be cleaned)
- `WINDOWS_ROAMING_DATA_PATH`: Roaming folder (syncs across devices)

## Building

### Prerequisites

1. Windows 10 SDK (10.0.19041.0 or higher)
2. Visual Studio 2022 with:
   - Desktop development with C++
   - Universal Windows Platform development
3. React Native Windows in your project

### Build Commands

Debug build:
```bash
msbuild windows\OPSQLite.sln /p:Configuration=Debug /p:Platform=x64
```

Release build:
```bash
msbuild windows\OPSQLite.sln /p:Configuration=Release /p:Platform=x64
```

### Supported Architectures

- x86 (32-bit)
- x64 (64-bit)  
- ARM64

## Integration

The module supports React Native Windows auto-linking. After installing the package:

```bash
npx react-native autolink-windows
```

This will automatically link the native module to your Windows app.

## Features

### Currently Supported
- ✅ Full SQLite core functionality
- ✅ JSI bindings for high performance
- ✅ Prepared statements
- ✅ Batch operations
- ✅ Reactive queries
- ✅ In-memory databases
- ✅ FTS5 (Full-Text Search)
- ✅ R-Tree indexes
- ✅ JSON support

### Pending Implementation
- ⏳ SQLCipher (encryption) - requires Windows build
- ⏳ cr-sqlite - requires Windows build
- ⏳ sqlite-vec - requires Windows build
- ⏳ Custom tokenizers

## Troubleshooting

### Common Issues

1. **Module not found after installation**
   - Run `npx react-native autolink-windows`
   - Clean and rebuild the solution

2. **Build errors**
   - Ensure all Visual Studio components are installed
   - Check Windows SDK version compatibility

3. **Database path issues**
   - Always use provided path constants
   - Don't hardcode paths - Windows apps are sandboxed

## Testing

Run the test script:
```bash
./scripts/test-windows.sh
```

Run with example app:
```bash
./scripts/test-windows.sh --run-example
```

## Contributing

When modifying Windows-specific code:

1. Keep platform-specific code minimal
2. Share C++ code with other platforms when possible
3. Test on both Debug and Release configurations
4. Test on x64 and ARM64 if possible

## Notes

- The Windows implementation reuses the existing C++ core from `cpp/`
- JSI bindings are installed on the JavaScript thread
- File operations use Windows Storage APIs for proper sandboxing
- The module is built as a Windows Runtime Component (WinRT)
