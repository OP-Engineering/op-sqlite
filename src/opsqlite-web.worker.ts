// This file runs inside a dedicated Web Worker. It is bundled by Metro as its
// own chunk via `new Worker(new URL("./opsqlite-web.worker", ...))`, so it
// must be self-contained: it cannot rely on globals set up by the app's main
// bundle (e.g. Expo's `import.meta.url` polyfill, which is only installed on
// the main thread).
declare const self: any;

// Metro (via Expo's `import.meta` Babel transform) rewrites every
// `import.meta.url` in this worker's dependency graph -- including inside
// @sqlite.org/sqlite-wasm itself -- into `globalThis.__ExpoImportMetaRegistry.url`.
// That registry is only installed by Expo's main-thread runtime, so without
// this shim any incidental `import.meta.url` read here throws
// "Cannot read properties of undefined (reading 'url')" before we ever get a
// chance to run. We supply our own wasm binary via `locateFile` below, so the
// value only needs to be non-throwing, not meaningful.
if (typeof (globalThis as any).__ExpoImportMetaRegistry === "undefined") {
  (globalThis as any).__ExpoImportMetaRegistry = { url: self.location?.href ?? "" };
}

import * as sqliteWasmModule from "@sqlite.org/sqlite-wasm";
const sqlite3InitModule: any = (sqliteWasmModule as any).default;

type WorkerRequest = {
  id: number;
  type: "open" | "close" | "exec";
  payload: Record<string, any>;
};

// Locating the wasm binary differs by bundler, and there's no single import
// spelling both agree on:
//  - Vite/webpack (real ESM): the library's own `new URL("sqlite3.wasm",
//    import.meta.url)` fallback works natively (import.meta.url is a real,
//    per-module value there), and a static `import` of a raw .wasm file is
//    actively rejected by Vite unless it's the wasm-as-ESM proposal.
//  - Metro (Expo web): import.meta.url is rewritten to a single shared value
//    for the whole bundled worker chunk (see the shim above), so the
//    library's fallback resolves to a bogus URL. It needs an explicit
//    `locateFile` pointing at a bundler-resolved asset instead. A *dynamic*
//    `import()` of that asset from within this worker chunk fails at
//    runtime ("Requiring unknown module") -- Metro's async `import()`
//    machinery expects a separately fetchable chunk, which asset modules
//    aren't -- so the fallback below uses `require()` (a synchronous,
//    same-bundle dependency in Metro's model, exactly like our top-level
//    imports already compile to) instead. Vite's static import analysis
//    only hooks `import`/`import()`, not `require`, so it never touches
//    this call -- and it's never reached at runtime either, since Vite's
//    native path always succeeds first.
declare const require: any;

async function initSqlite3(): Promise<any> {
  try {
    return await sqlite3InitModule();
  } catch {
    const wasmAssetUrl = require("./opsqlite-web-wasm-asset").default;
    return await sqlite3InitModule({ locateFile: () => wasmAssetUrl });
  }
}

let sahPoolPromise: Promise<{ sqlite3: any; OpfsSAHPoolDb: any }> | null = null;
const openDbs = new Map<string, any>();

async function getSahPool(): Promise<{ sqlite3: any; OpfsSAHPoolDb: any }> {
  if (!sahPoolPromise) {
    sahPoolPromise = initSqlite3().then(async (sqlite3: any) => {
      const poolUtil = await sqlite3.installOpfsSAHPoolVfs({ name: "opsqlite-opfs-sahpool" });
      return { sqlite3, OpfsSAHPoolDb: poolUtil.OpfsSAHPoolDb };
    });
  }
  return sahPoolPromise!;
}

function getDb(dbId: string): any {
  const db = openDbs.get(dbId);
  if (!db) {
    throw new Error(`[op-sqlite] No open web database for id "${dbId}"`);
  }
  return db;
}

async function handleRequest(type: WorkerRequest["type"], payload: Record<string, any>): Promise<any> {
  const { sqlite3, OpfsSAHPoolDb } = await getSahPool();

  switch (type) {
    case "open": {
      const db = new OpfsSAHPoolDb(payload.filename);
      openDbs.set(payload.dbId, db);
      return {};
    }

    case "close": {
      const db = openDbs.get(payload.dbId);
      db?.close();
      openDbs.delete(payload.dbId);
      return {};
    }

    case "exec": {
      const db = getDb(payload.dbId);
      const columnNames: string[] = [];
      const resultRows: any[] = [];

      db.exec({
        sql: payload.sql,
        bind: payload.bind,
        rowMode: payload.rowMode === "array" ? "array" : "object",
        columnNames,
        resultRows,
      });

      return {
        resultRows,
        columnNames,
        changeCount: db.changes(),
        lastInsertRowId: sqlite3.capi.sqlite3_last_insert_rowid(db.pointer),
      };
    }

    default:
      throw new Error(`[op-sqlite] Unknown web worker message type: ${type}`);
  }
}

self.onmessage = async (event: { data: WorkerRequest }) => {
  const { id, type, payload } = event.data;

  try {
    const result = await handleRequest(type, payload ?? {});
    self.postMessage({ id, result });
  } catch (error) {
    self.postMessage({ id, error: { message: (error as Error).message } });
  }
};
