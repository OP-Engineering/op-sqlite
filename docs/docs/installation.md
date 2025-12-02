---
sidebar_position: 1
---

# Installation

Just install and run. In the latest RN versions, pod installation should be automatic but if not run `pod install` manually.

```bash
npm i -s @op-engineering/op-sqlite
```

You cannot use this library on a expo-go app, you need to pre-build your app. No expo plugin is needed, just make sure the pods are properly setup.

```bash
npx expo install @op-engineering/op-sqlite
npx expo prebuild --clean
```

This package only runs on `iOS`, `Android` and `macOS`.

# Configuration

SQLite is very customizable on compilation level. op-sqlite also allows you add extensions or even change the base implementation. You can do this by adding the following to your `package.json`:

> [!IMPORTANT]
> When using a monorepo, be sure to add the "op-sqlite" config to the monorepo root `package.json` file.
> This is necessary because the `node_modules/@op-engineering/op-sqlite/op-sqlite.podspec` will search for first the `package.json` file in a parent directory.
> Alternative, you may be able to solve this by blocking this package from being hoisted to the root `node_modules`.
> Check your ios/Podfile.lock to see where it's being installed.

```json
{
  // ... the rest of your package.json
  // All the keys are optional, see the usage below
  "op-sqlite": {
    "sqlcipher": false
    // "crsqlite": false,
    // "performanceMode": true,
    // "iosSqlite": false,
    // "sqliteFlags": "-DSQLITE_DQS=0 -DSQLITE_MY_FLAG=1",
    // "fts5": true,
    // "rtree": true,
    // "libsql": true,
    // "sqliteVec": true,
    // "tokenizers": ["simple_tokenizer"]
  }
}
```

All keys are optional, only turn on the features you want:

- `sqlcipher` allows to change the base sqlite implementation to [sqlcipher](https://www.zetetic.net/sqlcipher/), which encrypts all the database data with minimal overhead. You will still need to keep your encryption key secure. Read more about security in React Native [here](https://ospfranco.com/react-native-security-guide/).
- `crsqlite` is an extension that allows replication to a server backed sqlite database copy. [Repo here](https://github.com/vlcn-io/cr-sqlite).
- `performanceMode` turns on certain compilation flags that make sqlite speedier at the cost of disabling some features. You should almost always turn this on, but test your app thoroughly.
- `iosSqlite` uses the embedded iOS version from sqlite, which saves disk space but may use an older version and cannot load extensions as Apple disables it due to security concerns. On Android SQLite is always compiled from source as each vendor messes with sqlite or uses outdated versions.
- `sqliteFlags` allows you to pass your own compilation flags to further disable/enable features and extensions. It follows the C flag format: `-D[YOUR_FLAG]=[YOUR_VALUE]`. If you are running large queries on large databases sometimes on Android devices you might get a IO exception. You can disable temporary files by using adding the `"-DSQLITE_TEMP_STORE=2"` flag.
- `fts5` enables the full [text search extension](https://www.sqlite.org/fts5.html).
- `tokenizers` allows you to write your own C tokenizers. Read more in the corresponding section in this documentation.
- `rtree` enables the [rtree extension](https://www.sqlite.org/rtree.html)
- `sqliteVec` enables [sqlite-vec](https://github.com/asg017/sqlite-vec), an extension for RAG embeddings

Some combination of features are not allowed. For example `sqlcipher` and `iosSqlite` since they are fundamentally different sources. In this cases you will get an error while doing a pod install or during the Android build.

# Conflicts

## use_frameworks

In case you are using `use_frameworks` (for example if you are using `react-native-firebase`), this will break the compilation process and force the compilation to use the embedded sqlite on iOS. One possible workaround is forcing `op-sqlite` to be compiled statically, you can modify the `Podfile` as:

```ruby
pre_install do |installer|
  installer.pod_targets.each do |pod|
    if pod.name.eql?('op-sqlite')
      def pod.build_type
        Pod::BuildType.static_library
      end
    end
  end
end
```

If you are using Expo CNG you can use the [`expo-plugin-ios-static-libraries`](https://github.com/jonshaffer/expo-plugin-ios-static-libraries?tab=readme-ov-file#installation) plugin to automate adding the static setting for op-sqlite

It forces static compilation on `op-sqlite` only. Since everything is compiled from sources this _should_ work, however do it at your own risk since other compilation errors might arise.

## Compilation clashes

If you have other packages that are dependent on sqlite you will have issues. Some of the known offenders are:

- `expo-updates`
- `expo-sqlite`
- `cozodb`
- Other packages that try to add/link sqlite (or the sqlite3 cocoapod)

### Expo Updates

`expo-updates` now has a added a new way to avoid a hard dependency on sqlite. Adding `"expo.updates.useThirdPartySQLitePod": "true"` to `ios/Podfile.properties.json` fixes the duplicate symbols and header definition issues when `expo-updates` is the only conflicting package.

An expo plugin can also be used:

```jsx
import type { ConfigPlugin } from '@expo/config-plugins';
import { withPodfileProperties } from '@expo/config-plugins';

const withUseThirdPartySQLitePod: ConfigPlugin<never> = (expoConfig) => {
  return withPodfileProperties(expoConfig, (config) => {
    config.modResults = {
      ...config.modResults,
      'expo.updates.useThirdPartySQLitePod': 'true',
    };
    return config;
  });
};

export default withUseThirdPartySQLitePod;
```

If you cannot remove the dependency each of the packages will try to compile sqlite from sources or link it on build time. Even if they manage to compile, they might compile sqlite with different compilation flags and you might face runtime errors.

Another workaround for the expo packages, is you can use the iOS embedded version of sqlite for op-sqlite, in your `package.json` turn on the `iosSqlite` flag:

```json
"op-sqlite": {
  "iosSqlite": true
}
```

This means however, you will be used whatever version the phone is running, which might be outdated and it also does not support extension loading. There is no way around this.

### Libsql and Expo Updates

If you want to use `expo-updates` and `libsql` at the same time there is one more workaround you need to apply. On your `AppDelegate` (or wherever you initialize your RN view if it's a brownfield integration), you need to call `[OPSQLite expoUpdatesWorkaround];` before initializing the RN view. In case of a normal expo app modify the `AppDelegate.mm` as follows:

```objective-c
#import "OPSQLite.h" // Add the header

// Modify the didFinishLaunchingWithOptions function
-(BOOL)application: (UIApplication *)application didFinishLaunchingWithOptions: (NSDictionary *)launchOptions {
  self moduleName = @"main";
  self.initialProps = 0{};

  // Add the call to the workaround
  [OPSQLite expoUpdatesWorkaround];

  return [super application:application didFinishLaunchingWithOptions:launchOptions];
}
```

## [Error: Base module not found. Did you do a pod install/clear the gradle cache?]

If you are running into this issue, you have to clear the gradle cache. It is stored inside of the [gradle user home directory](https://docs.gradle.org/current/userguide/directory_layout.html#dir:gradle_user_home), in the `caches/` directory. Removing the `caches/` directory and rebuilding your app should be enough to fix the error.

# Other

For other conflicts and compilation errors there is no documented solutions. You need to get rid of the double compilation by hand, either by patching the compilation of each package so that it still builds or removing the dependency on the package.

On Android you might be able to get away by just [using a `pickFirst` strategy](https://ospfranco.com/how-to-resolve-duplicated-libraries-on-android/). On iOS depending on the build system you might be able to patch it via a post-build hook, something like:

```ruby
pre_install do |installer|
	installer.pod_targets.each do |pod|
		if pod.name.eql?('expo-updates')
			# Modify the configuration of the pod so it doesn't depend on the sqlite pod
		end
	end
end
```
