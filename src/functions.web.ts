import type {
  _InternalDB,
  _PendingTransaction,
  BatchQueryResult,
  DB,
  DBParams,
  FileLoadResult,
  OpenOptions,
  OPSQLiteProxy,
  PreparedStatement,
  QueryResult,
  RawQueryResult,
  Scalar,
  SQLBatchTuple,
  Transaction,
} from "./types";

type WorkerPromiser = (type: string, args?: Record<string, unknown>) => Promise<any>;

const WEB_ONLY_SYNC_ERROR =
  "[op-sqlite] Web backend is async-only. Use openAsync() and async methods like execute().";

function throwSyncApiError(method: string): never {
  throw new Error(`${WEB_ONLY_SYNC_ERROR} Called sync method: ${method}().`);
}

function toNumber(value: unknown): number | undefined {
  if (value == null) {
    return undefined;
  }

  if (typeof value === "bigint") {
    const asNumber = Number(value);
    return Number.isFinite(asNumber) ? asNumber : undefined;
  }

  if (typeof value === "number") {
    return Number.isFinite(value) ? value : undefined;
  }

  return undefined;
}

function ensureSingleStatement(sql: string): void {
  // Web worker executes the full SQL string while native executes only the first prepared statement.
  // We warn here so callers can keep behavior consistent across platforms when needed.
  if (sql.includes(";")) {
    const trimmed = sql.trim();
    if (!trimmed.endsWith(";") || trimmed.slice(0, -1).includes(";")) {
      console.warn(
        "[op-sqlite] Web execute() runs full SQL strings. Avoid multi-statement SQL for parity with native first-statement behavior.",
      );
    }
  }
}

// Both Metro (Expo web) and Vite only statically discover a worker's URL
// when it's written as a literal `new Worker(new URL("./relative", import.meta.url))`
// expression -- this code runs on the main thread, where Expo's import.meta
// polyfill *is* installed, so it's safe to use here (unlike inside the
// worker itself; see opsqlite-web.worker.ts for why that file can't).
// @sqlite.org/sqlite-wasm's own bundled worker also uses this pattern, but
// with a bare (non-"./") specifier that Metro can't resolve, and it performs
// *other*, unrelated `import.meta.url` reads inside the worker where the
// polyfill is missing -- which is why we ship our own tiny worker built
// against its lower-level API instead of using its worker directly.
//
// Metro's worker bundling only recognizes a *bare* `Worker` identifier as the
// `new Worker(...)` callee (a `globalThis.Worker`/cast member expression does
// not match), so `Worker` is declared as an ambient global here instead of
// accessed through a cast -- that keeps this file type-checkable without
// pulling in the DOM lib while still emitting the exact AST shape Metro's
// static analysis looks for.
declare const Worker: any;

let worker: any = null;
let nextRequestId = 0;
const pendingRequests = new Map<number, { resolve: (value: any) => void; reject: (error: Error) => void }>();

function getWorker(): any {
  if (!worker) {
    worker = new Worker(new URL("./opsqlite-web.worker", import.meta.url), { type: "module" });

    worker.onmessage = (event: { data: { id: number; result?: any; error?: { message: string } } }) => {
      const { id, result, error } = event.data;
      const pending = pendingRequests.get(id);
      if (!pending) {
        return;
      }

      pendingRequests.delete(id);
      if (error) {
        pending.reject(new Error(error.message));
      } else {
        pending.resolve(result);
      }
    };

    worker.onerror = (event: { message?: string }) => {
      const error = new Error(
        `[op-sqlite] Web worker failed to load. Make sure @sqlite.org/sqlite-wasm is installed (npm i @sqlite.org/sqlite-wasm) and that your bundler treats *.wasm as an asset (add "wasm" to metro.config.js's resolver.assetExts). Original error: ${event.message ?? "unknown"}`,
      );
      for (const pending of pendingRequests.values()) {
        pending.reject(error);
      }
      pendingRequests.clear();
    };
  }

  return worker;
}

const callWorker: WorkerPromiser = (type, payload) => {
  const w = getWorker();
  const id = nextRequestId++;

  return new Promise((resolve, reject) => {
    pendingRequests.set(id, { resolve, reject });
    w.postMessage({ id, type, payload });
  });
};

async function executeWorker(
  promiser: WorkerPromiser,
  dbId: string,
  query: string,
  params?: Scalar[],
): Promise<QueryResult> {
  ensureSingleStatement(query);

  let result: any;
  try {
    result = await promiser("exec", {
      dbId,
      sql: query,
      bind: params,
      rowMode: "object",
    });
  } catch (error) {
    throw new Error(
      `[op-sqlite] Web query failed. Ensure COOP/COEP headers are set and OPFS is available in this browser. Original error: ${(error as Error).message}`,
    );
  }

  const rows = Array.isArray(result?.resultRows) ? (result.resultRows as Array<Record<string, Scalar>>) : [];
  const columnNames = Array.isArray(result?.columnNames)
    ? (result.columnNames as string[])
    : rows.length > 0
      ? Object.keys(rows[0] ?? {})
      : [];

  const rowsAffected = toNumber(result?.changeCount) ?? 0;
  const insertId = toNumber(result?.lastInsertRowId);

  return {
    rowsAffected,
    insertId,
    rows,
    columnNames,
  };
}

function enhanceWebDb(db: _InternalDB, options: { name?: string; location?: string }): DB {
  const lock = {
    queue: [] as _PendingTransaction[],
    inProgress: false,
  };

  const startNextTransaction = () => {
    if (lock.inProgress || lock.queue.length === 0) {
      return;
    }

    lock.inProgress = true;
    const tx = lock.queue.shift();
    if (!tx) {
      throw new Error("Could not get an operation on database");
    }

    setTimeout(() => {
      tx.start();
    }, 0);
  };

  const withTransactionLock = async <T>(work: () => Promise<T>): Promise<T> => {
    return new Promise<T>((resolve, reject) => {
      const tx: _PendingTransaction = {
        start: () => {
          work()
            .then(resolve)
            .catch(reject)
            .finally(() => {
              lock.inProgress = false;
              startNextTransaction();
            });
        },
      };

      lock.queue.push(tx);
      startNextTransaction();
    });
  };

  const unsupported = (method: string) => () => throwSyncApiError(method);

  const enhancedDb: DB = {
    close: unsupported("close"),
    closeAsync: async () => {
      await db.closeAsync?.();
    },
    interrupt: unsupported("interrupt"),
    delete: unsupported("delete"),
    attach: unsupported("attach"),
    detach: unsupported("detach"),
    transaction: async (fn: (tx: Transaction) => Promise<void>): Promise<void> => {
      return withTransactionLock(async () => {
        let finalized = false;

        const commit = async (): Promise<QueryResult> => {
          if (finalized) {
            throw new Error(
              `OP-Sqlite Error: Database: ${options.name}. Cannot execute query on finalized transaction`,
            );
          }

          const res = await enhancedDb.execute("COMMIT;");
          finalized = true;
          return res;
        };

        const rollback = (): QueryResult => {
          throwSyncApiError("rollback");
        };

        const execute = async (query: string, params?: Scalar[]) => {
          if (finalized) {
            throw new Error(
              `OP-Sqlite Error: Database: ${options.name}. Cannot execute query on finalized transaction`,
            );
          }

          return enhancedDb.execute(query, params);
        };

        await enhancedDb.execute("BEGIN TRANSACTION;");

        try {
          await fn({
            execute,
            commit,
            rollback,
          });

          if (!finalized) {
            await commit();
          }
        } catch (error) {
          if (!finalized) {
            await enhancedDb.execute("ROLLBACK;");
          }

          throw error;
        }
      });
    },
    executeSync: unsupported("executeSync"),
    execute: db.execute,
    executeWithHostObjects: db.execute,
    executeBatch: async (commands: SQLBatchTuple[]): Promise<BatchQueryResult> => {
      await withTransactionLock(async () => {
        await db.execute("BEGIN TRANSACTION;");

        try {
          for (const command of commands) {
            const [sql, bind] = command;

            if (!bind) {
              await db.execute(sql);
              continue;
            }

            if (Array.isArray(bind[0])) {
              for (const rowBind of bind as Scalar[][]) {
                await db.execute(sql, rowBind);
              }
            } else {
              await db.execute(sql, bind as Scalar[]);
            }
          }

          await db.execute("COMMIT;");
        } catch (error) {
          await db.execute("ROLLBACK;");
          throw error;
        }
      });

      return {
        rowsAffected: 0,
      };
    },
    loadFile: async (_location: string): Promise<FileLoadResult> => {
      throw new Error("[op-sqlite] loadFile() is not supported on web.");
    },
    updateHook: () => {
      throw new Error("[op-sqlite] updateHook() is not supported on web.");
    },
    commitHook: () => {
      throw new Error("[op-sqlite] commitHook() is not supported on web.");
    },
    rollbackHook: () => {
      throw new Error("[op-sqlite] rollbackHook() is not supported on web.");
    },
    prepareStatement: (query: string): PreparedStatement => {
      let currentParams: Scalar[] = [];

      return {
        bind: async (params: Scalar[]) => {
          currentParams = params;
        },
        bindSync: unsupported("bindSync"),
        execute: async () => {
          return db.execute(query, currentParams);
        },
        executeSync: unsupported("executeSync"),
      };
    },
    loadExtension: unsupported("loadExtension"),
    executeRaw: db.executeRaw,
    executeRawSync: unsupported("executeRawSync"),
    getDbPath: unsupported("getDbPath"),
    reactiveExecute: unsupported("reactiveExecute"),
    sync: unsupported("sync"),
    setReservedBytes: unsupported("setReservedBytes"),
    getReservedBytes: unsupported("getReservedBytes"),
    flushPendingReactiveQueries: async () => {},
  };

  return enhancedDb;
}

async function createWebDb(params: OpenOptions): Promise<_InternalDB> {
  if (params.encryptionKey) {
    throw new Error("[op-sqlite] SQLCipher is not supported on web.");
  }

  const promiser = callWorker;

  const dbId = `${params.name}-${Date.now()}-${Math.random().toString(36).slice(2)}`;
  try {
    await promiser("open", {
      dbId,
      filename: params.name,
    });
  } catch (error) {
    throw new Error(
      `[op-sqlite] Failed to open web sqlite database. Ensure COOP/COEP headers are set and OPFS is available in this browser. Original error: ${(error as Error).message}`,
    );
  }

  return {
    close: () => {
      throwSyncApiError("close");
    },
    closeAsync: async () => {
      await promiser("close", {
        dbId,
      });
    },
    interrupt: () => {
      throwSyncApiError("interrupt");
    },
    delete: () => {
      throwSyncApiError("delete");
    },
    attach: () => {
      throw new Error("[op-sqlite] attach() is not supported on web.");
    },
    detach: () => {
      throw new Error("[op-sqlite] detach() is not supported on web.");
    },
    transaction: async () => {
      throw new Error("[op-sqlite] transaction() must be called on an opened DB object.");
    },
    executeSync: () => {
      throwSyncApiError("executeSync");
    },
    execute: async (query: string, bind?: Scalar[]) => {
      return executeWorker(promiser, dbId, query, bind);
    },
    executeWithHostObjects: async (query: string, bind?: Scalar[]) => {
      return executeWorker(promiser, dbId, query, bind);
    },
    executeBatch: async (_commands: SQLBatchTuple[]) => {
      throw new Error("[op-sqlite] executeBatch() must be called on an opened DB object.");
    },
    loadFile: async (_location: string) => {
      throw new Error("[op-sqlite] loadFile() is not supported on web.");
    },
    updateHook: () => {
      throw new Error("[op-sqlite] updateHook() is not supported on web.");
    },
    commitHook: () => {
      throw new Error("[op-sqlite] commitHook() is not supported on web.");
    },
    rollbackHook: () => {
      throw new Error("[op-sqlite] rollbackHook() is not supported on web.");
    },
    prepareStatement: (_query: string) => {
      throw new Error("[op-sqlite] prepareStatement() must be called on an opened DB object.");
    },
    loadExtension: () => {
      throw new Error("[op-sqlite] loadExtension() is not supported on web.");
    },
    executeRaw: async (query: string, bind?: Scalar[]): Promise<RawQueryResult> => {
      ensureSingleStatement(query);

      const result = await promiser("exec", {
        dbId,
        sql: query,
        bind,
        rowMode: "array",
      });

      const rawRows = Array.isArray(result?.resultRows) ? (result.resultRows as Scalar[][]) : [];
      const columnNames = Array.isArray(result?.columnNames) ? (result.columnNames as string[]) : [];

      return {
        rowsAffected: toNumber(result?.changeCount) ?? 0,
        insertId: toNumber(result?.lastInsertRowId),
        rawRows,
        columnNames,
      };
    },
    executeRawSync: () => {
      throwSyncApiError("executeRawSync");
    },
    getDbPath: () => {
      throwSyncApiError("getDbPath");
    },
    reactiveExecute: () => {
      throw new Error("[op-sqlite] reactiveExecute() is not supported on web.");
    },
    sync: () => {
      throwSyncApiError("sync");
    },
    setReservedBytes: () => {
      throwSyncApiError("setReservedBytes");
    },
    getReservedBytes: () => {
      throwSyncApiError("getReservedBytes");
    },
    flushPendingReactiveQueries: async () => {},
  };
}

/**
 * Open a connection to a local sqlite database on web.
 * Web is async-only: use openAsync() and async methods like execute().
 */
export const openAsync = async (params: OpenOptions): Promise<DB> => {
  const db = await createWebDb(params);
  return enhanceWebDb(db, params);
};

export const open = (_params:OpenOptions): DB => {
  throwSyncApiError("open");
};

export const openSync = (_params: {
  url: string;
  authToken: string;
  name: string;
  location?: string;
  libsqlSyncInterval?: number;
  libsqlOffline?: boolean;
  encryptionKey?: string;
  remoteEncryptionKey?: string;
}): DB => {
  throwSyncApiError("openSync");
};

export const openRemote = (_params: { url: string; authToken: string }): DB => {
  throw new Error("[op-sqlite] openRemote() is not supported on web.");
};

export const moveAssetsDatabase = async (_args: {
  filename: string;
  path?: string;
  overwrite?: boolean;
}): Promise<boolean> => {
  throw new Error("[op-sqlite] moveAssetsDatabase() is not supported on web.");
};

export const getDylibPath = (_bundle: string, _name: string): string => {
  throw new Error("[op-sqlite] getDylibPath() is not supported on web.");
};

export const isSQLCipher = (): boolean => {
  return false;
};

export const isLibsql = (): boolean => {
  return false;
};

export const isTurso = (): boolean => {
  return false;
};

export const isIOSEmbedded = (): boolean => {
  return false;
};

/**
 * @deprecated Use `isIOSEmbedded` instead. This alias will be removed in a future release.
 */
export const isIOSEmbeeded = isIOSEmbedded;

// Web does not expose the native JSI proxy object.
export const OPSQLite = {} as OPSQLiteProxy;
