# TypeScript NodeNext: `open` (and others) not visible on `@op-engineering/op-sqlite` namespace import

- Package: `@op-engineering/op-sqlite@15.0.3`
- Affected exports: `open`, `openSync`, `openRemote` (and other value exports
  from `./functions`)
- Environment:
  - TypeScript: 5.8.3
  - tsconfig: `"module": "NodeNext"`, `"moduleResolution": "NodeNext"`
  - React Native: 0.80.x

## Repro

```ts
// consumer tsconfig: moduleResolution NodeNext
import * as Sqlite from "@op-engineering/op-sqlite";

// TS2339: Property 'open' does not exist on type 'typeof import(".../op-sqlite/lib/typescript/src/index")'.
Sqlite.open({ name: "test.db" });
```

Also fails with named imports:

```ts
// TS2305: Module '"@op-engineering/op-sqlite"' has no exported member 'open'.
import { open } from "@op-engineering/op-sqlite";
```

## Expected

The `open`/`openSync`/`openRemote` functions should be visible from the package
root (they exist at runtime and are present in `lib/module/functions.js`). The
type definitions should expose these values so the above code type‑checks in
NodeNext projects.

## What’s happening

- The package `types` entry points to `lib/typescript/src/index.d.ts` which
  contains:
  - `export * from './functions'`
  - `export { Storage } from './Storage'`
  - `export * from './types'`
- Under `moduleResolution: NodeNext`, TypeScript can elide star re‑exports
  coming from internal subpaths that aren’t explicitly exposed through the
  package’s `exports` map. As a result, the namespace import’s type shape does
  not include the value exports from `./functions` (e.g. `open`), even though
  they exist in `lib/typescript/src/functions.d.ts` and at runtime.
- Direct exports (e.g. `Storage`) still appear, which matches the observed
  behavior: `Storage` is available, `open` is not.

Notes

- Runtime ESM (`lib/module/index.js`) does `export * from "./functions.js"`, so
  behavior at runtime is correct. This is a type‑only visibility issue.

## Concrete fix plan (upstream)

Option A — Explicit named re‑exports (minimal change)

1. Update `src/index.ts` to explicitly re‑export the functions instead of
   `export *`.
   ```ts
   // src/index.ts
   import { NativeModules } from "react-native";

   export { Storage } from "./Storage";
   export * from "./types";
   export {
     getDylibPath,
     isIOSEmbeeded, // keep name as-is for back-compat (note typo)
     isLibsql,
     isSQLCipher,
     moveAssetsDatabase,
     open,
     openRemote,
     openSync,
     OPSQLite,
   } from "./functions";

   export const {
     IOS_DOCUMENT_PATH,
     IOS_LIBRARY_PATH,
     ANDROID_DATABASE_PATH,
     ANDROID_FILES_PATH,
     ANDROID_EXTERNAL_FILES_PATH,
   } = !!NativeModules.OPSQLite.getConstants
     ? NativeModules.OPSQLite.getConstants()
     : NativeModules.OPSQLite;
   ```
2. Rebuild so `lib/typescript/src/index.d.ts` emits named re‑exports. This
   avoids NodeNext re‑export elision and surfaces the values to consumers.

Option B — Add subpath exports for types (defense‑in‑depth)

- In `package.json`, add subpaths for internal modules referenced by the types
  entry. This allows TypeScript’s NodeNext resolver to “see” those modules
  explicitly and has no breaking impact on existing imports:
  ```json
  {
    "exports": {
      ".": {
        "source": "./src/index.ts",
        "types": "./lib/typescript/src/index.d.ts",
        "default": "./lib/module/index.js"
      },
      "./functions": {
        "types": "./lib/typescript/src/functions.d.ts",
        "default": "./lib/module/functions.js"
      },
      "./types": {
        "types": "./lib/typescript/src/types.d.ts",
        "default": "./lib/module/types.js"
      },
      "./Storage": {
        "types": "./lib/typescript/src/Storage.d.ts",
        "default": "./lib/module/Storage.js"
      },
      "./package.json": "./package.json"
    }
  }
  ```
- This is optional if Option A is applied, but increases robustness across TS
  versions and resolvers.

Option C — Optional follow‑up (naming)

- There is a spelling mismatch: public function is exported as `isIOSEmbeeded`
  while the proxy method is `isIOSEmbedded`. Consider exporting both names to
  avoid breaking changes:
  ```ts
  export const isIOSEmbedded = isIOSEmbeeded;
  ```

## Verification

- Add an integration workspace to the repo with a minimal TS project using
  `moduleResolution: NodeNext` that does:
  ```ts
  import * as Sqlite from "@op-engineering/op-sqlite";
  const db = Sqlite.open({ name: "test" });
  db.close();
  ```
- Run `tsc --noEmit` in that project and ensure no TS errors. Also verify named
  imports compile:
  ```ts
  import { open } from "@op-engineering/op-sqlite";
  ```

## Workarounds for consumers (until a fix is published)

- Add a local ambient module augmentation:
  ```ts
  // op-sqlite-augmentation.d.ts
  declare module "@op-engineering/op-sqlite" {
    export function open(
      options: { name: string; location?: string; encryptionKey?: string },
    ): import("@op-engineering/op-sqlite").DB;
    export function openSync(
      params: {
        url: string;
        authToken: string;
        name: string;
        location?: string;
        libsqlSyncInterval?: number;
        libsqlOffline?: boolean;
        encryptionKey?: string;
        remoteEncryptionKey?: string;
      },
    ): import("@op-engineering/op-sqlite").DB;
    export function openRemote(
      params: { url: string; authToken: string },
    ): import("@op-engineering/op-sqlite").DB;
  }
  ```
- Alternatively use `moduleResolution: 'Bundler'` (looser) in consumer tsconfig,
  if feasible.

## Impact

- Blocks type‑safe usage of `open`/`openSync`/`openRemote` when using NodeNext
  resolution (common in RN/modern ESM set‑ups). Runtime works, but users get
  TS2339/TS2305 until they add manual augmentations.

---

Happy to send a PR with Option A (and B if desired). Let me know your
preference.
