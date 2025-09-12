import { NativeModules, Platform } from 'react-native';
import {
  type _InternalDB,
  type _PendingTransaction,
  type BatchQueryResult,
  type DB,
  type DBParams,
  type OPSQLiteProxy,
  type QueryResult,
  type Scalar,
  type SQLBatchTuple,
  type Transaction,
} from './types';

declare global {
  var __OPSQLiteProxy: object | undefined;
}

if (global.__OPSQLiteProxy == null) {
  if (NativeModules.OPSQLite == null) {
    throw new Error(
      'Base module not found. Did you do a pod install/clear the gradle cache?'
    );
  }

  // Call the synchronous blocking install() function
  const installed = NativeModules.OPSQLite.install();
  if (!installed) {
    throw new Error(
      `Failed to install op-sqlite: The native OPSQLite Module could not be installed! Looks like something went wrong when installing JSI bindings, check the native logs for more info`
    );
  }

  // Check again if the constructor now exists. If not, throw an error.
  if (global.__OPSQLiteProxy == null) {
    throw new Error(
      'OPSqlite native object is not available. Something is wrong. Check the native logs for more information.'
    );
  }
}

const proxy = global.__OPSQLiteProxy;
export const OPSQLite = proxy as OPSQLiteProxy;

function enhanceDB(db: _InternalDB, options: DBParams): DB {
  const lock = {
    queue: [] as _PendingTransaction[],
    inProgress: false,
  };

  const startNextTransaction = () => {
    if (lock.inProgress) {
      // Transaction is already in process bail out
      return;
    }

    if (lock.queue.length) {
      lock.inProgress = true;
      const tx = lock.queue.shift();

      if (!tx) {
        throw new Error('Could not get a operation on database');
      }

      setImmediate(() => {
        tx.start();
      });
    }
  };

  function sanitizeArrayBuffersInArray(
    params?: any[] | any[][]
  ): any[] | undefined {
    if (!params) {
      return params;
    }

    return params.map((p) => {
      if (Array.isArray(p)) {
        return sanitizeArrayBuffersInArray(p);
      }

      if (ArrayBuffer.isView(p)) {
        return p.buffer;
      }

      return p;
    });
  }

  // spreading the object does not work with HostObjects (db)
  // We need to manually assign the fields
  let enhancedDb = {
    delete: db.delete,
    attach: db.attach,
    detach: db.detach,
    executeBatch: async (
      commands: SQLBatchTuple[]
    ): Promise<BatchQueryResult> => {
      // Do normal for loop and replace in place for performance
      for (let i = 0; i < commands.length; i++) {
        // [1] is the params arg
        if (commands[i]![1]) {
          commands[i]![1] = sanitizeArrayBuffersInArray(commands[i]![1]) as any;
        }
      }

      async function run() {
        try {
          enhancedDb.executeSync('BEGIN TRANSACTION;');

          let res = await db.executeBatch(commands as any[]);

          enhancedDb.executeSync('COMMIT;');

          await db.flushPendingReactiveQueries();

          return res;
        } catch (executionError) {
          try {
            enhancedDb.executeSync('ROLLBACK;');
          } catch (rollbackError) {
            throw rollbackError;
          }

          throw executionError;
        } finally {
          lock.inProgress = false;
          startNextTransaction();
        }
      }

      return await new Promise((resolve, reject) => {
        const tx: _PendingTransaction = {
          start: () => {
            run().then(resolve).catch(reject);
          },
        };

        lock.queue.push(tx);
        startNextTransaction();
      });
    },
    loadFile: db.loadFile,
    updateHook: db.updateHook,
    commitHook: db.commitHook,
    rollbackHook: db.rollbackHook,
    loadExtension: db.loadExtension,
    getDbPath: db.getDbPath,
    reactiveExecute: db.reactiveExecute,
    sync: db.sync,
    close: db.close,
    executeWithHostObjects: async (
      query: string,
      params?: Scalar[]
    ): Promise<QueryResult> => {
      const sanitizedParams = sanitizeArrayBuffersInArray(params);

      return sanitizedParams
        ? await db.executeWithHostObjects(query, sanitizedParams as Scalar[])
        : await db.executeWithHostObjects(query);
    },
    executeRaw: async (query: string, params?: Scalar[]) => {
      const sanitizedParams = sanitizeArrayBuffersInArray(params);

      return db.executeRaw(query, sanitizedParams as Scalar[]);
    },
    executeRawSync: (query: string, params?: Scalar[]) => {
      const sanitizedParams = sanitizeArrayBuffersInArray(params);
      return db.executeRawSync(query, sanitizedParams as Scalar[]);
    },
    // Wrapper for executeRaw, drizzleORM uses this function
    // at some point I changed the API but they did not pin their dependency to a specific version
    // so re-inserting this so it starts working again
    executeRawAsync: async (query: string, params?: Scalar[]) => {
      const sanitizedParams = sanitizeArrayBuffersInArray(params);

      return db.executeRaw(query, sanitizedParams as Scalar[]);
    },
    executeSync: (query: string, params?: Scalar[]): QueryResult => {
      const sanitizedParams = sanitizeArrayBuffersInArray(params);

      let intermediateResult = sanitizedParams
        ? db.executeSync(query, sanitizedParams as Scalar[])
        : db.executeSync(query);

      let rows: Record<string, Scalar>[] = [];
      for (let i = 0; i < (intermediateResult.rawRows?.length ?? 0); i++) {
        let row: Record<string, Scalar> = {};
        let rawRow = intermediateResult.rawRows![i]!;
        for (let j = 0; j < intermediateResult.columnNames!.length; j++) {
          let columnName = intermediateResult.columnNames![j]!;
          let value = rawRow[j]!;

          row[columnName] = value;
        }
        rows.push(row);
      }

      let res = {
        ...intermediateResult,
        rows,
      };

      delete res.rawRows;

      return res;
    },
    executeAsync: async (
      query: string,
      params?: Scalar[] | undefined
    ): Promise<QueryResult> => {
      return db.execute(query, params);
    },
    execute: async (
      query: string,
      params?: Scalar[] | undefined
    ): Promise<QueryResult> => {
      const sanitizedParams = sanitizeArrayBuffersInArray(params);

      let intermediateResult = await db.execute(
        query,
        sanitizedParams as Scalar[]
      );

      let rows: Record<string, Scalar>[] = [];
      for (let i = 0; i < (intermediateResult.rawRows?.length ?? 0); i++) {
        let row: Record<string, Scalar> = {};
        let rawRow = intermediateResult.rawRows![i]!;
        for (let j = 0; j < intermediateResult.columnNames!.length; j++) {
          let columnName = intermediateResult.columnNames![j]!;
          let value = rawRow[j]!;

          row[columnName] = value;
        }
        rows.push(row);
      }

      let res = {
        ...intermediateResult,
        rows,
      };

      delete res.rawRows;

      return res;
    },
    prepareStatement: (query: string) => {
      const stmt = db.prepareStatement(query);

      return {
        bindSync: (params: Scalar[]) => {
          const sanitizedParams = sanitizeArrayBuffersInArray(params);

          stmt.bindSync(sanitizedParams!);
        },
        bind: async (params: Scalar[]) => {
          const sanitizedParams = sanitizeArrayBuffersInArray(params);

          await stmt.bind(sanitizedParams!);
        },
        execute: stmt.execute,
      };
    },
    transaction: async (
      fn: (tx: Transaction) => Promise<void>
    ): Promise<void> => {
      let isFinalized = false;

      const execute = async (query: string, params?: Scalar[]) => {
        if (isFinalized) {
          throw Error(
            `OP-Sqlite Error: Database: ${
              options.name || options.url
            }. Cannot execute query on finalized transaction`
          );
        }
        return await enhancedDb.execute(query, params);
      };

      const commit = async (): Promise<QueryResult> => {
        if (isFinalized) {
          throw Error(
            `OP-Sqlite Error: Database: ${
              options.name || options.url
            }. Cannot execute query on finalized transaction`
          );
        }
        const result = enhancedDb.executeSync('COMMIT;');

        await db.flushPendingReactiveQueries();

        isFinalized = true;
        return result;
      };

      const rollback = (): QueryResult => {
        if (isFinalized) {
          throw Error(
            `OP-Sqlite Error: Database: ${
              options.name || options.url
            }. Cannot execute query on finalized transaction`
          );
        }
        const result = enhancedDb.executeSync('ROLLBACK;');
        isFinalized = true;
        return result;
      };

      async function run() {
        try {
          enhancedDb.executeSync('BEGIN TRANSACTION;');

          await fn({
            commit,
            execute,
            rollback,
          });

          if (!isFinalized) {
            commit();
          }
        } catch (executionError) {
          if (!isFinalized) {
            try {
              rollback();
            } catch (rollbackError) {
              throw rollbackError;
            }
          }

          throw executionError;
        } finally {
          lock.inProgress = false;
          isFinalized = false;
          startNextTransaction();
        }
      }

      return await new Promise((resolve, reject) => {
        const tx: _PendingTransaction = {
          start: () => {
            run().then(resolve).catch(reject);
          },
        };

        lock.queue.push(tx);
        startNextTransaction();
      });
    },
  };

  return enhancedDb;
}

/**
 * Open a replicating connection via libsql to a turso db
 * libsql needs to be enabled on your package.json
 */
export const openSync = (params: {
  url: string;
  authToken: string;
  name: string;
  location?: string;
  libsqlSyncInterval?: number;
  libsqlOffline?: boolean;
  encryptionKey?: string;
  remoteEncryptionKey?: string;
}): DB => {
  if (!isLibsql()) {
    throw new Error('This function is only available for libsql');
  }

  const db = OPSQLite.openSync(params);
  const enhancedDb = enhanceDB(db, params);

  return enhancedDb;
};

/**
 * Open a remote connection via libsql to a turso db
 * libsql needs to be enabled on your package.json
 */
export const openRemote = (params: { url: string; authToken: string }): DB => {
  if (!isLibsql()) {
    throw new Error('This function is only available for libsql');
  }

  const db = OPSQLite.openRemote(params);
  const enhancedDb = enhanceDB(db, params);

  return enhancedDb;
};

/**
 * Open a connection to a local sqlite or sqlcipher database
 * If you want libsql remote or sync connections, use openSync or openRemote
 */
export const open = (params: {
  name: string;
  location?: string;
  encryptionKey?: string;
}): DB => {
  if (params.location?.startsWith('file://')) {
    console.warn(
      "[op-sqlite] You are passing a path with 'file://' prefix, it's automatically removed"
    );
    params.location = params.location.substring(7);
  }

  const db = OPSQLite.open(params);
  const enhancedDb = enhanceDB(db, params);

  return enhancedDb;
};

/**
 * Moves the database from the assets folder to the default path (check the docs) or to a custom path
 * It DOES NOT OVERWRITE the database if it already exists in the destination path
 * if you want to overwrite the database, you need to pass the overwrite flag as true
 * @param args object with the parameters for the operaiton
 * @returns promise, rejects if failed to move the database, resolves if the operation was successful
 */
export const moveAssetsDatabase = async (args: {
  filename: string;
  path?: string;
  overwrite?: boolean;
}): Promise<boolean> => {
  return NativeModules.OPSQLite.moveAssetsDatabase(args);
};

/**
 * Used to load a dylib file that contains a sqlite 3 extension/plugin
 * It returns the raw path to the actual file which then needs to be passed to the loadExtension function
 * Check the docs for more information
 * @param bundle the iOS bundle identifier of the .framework
 * @param name the file name of the dylib file
 * @returns
 */
export const getDylibPath = (bundle: string, name: string): string => {
  return NativeModules.OPSQLite.getDylibPath(bundle, name);
};

export const isSQLCipher = (): boolean => {
  return OPSQLite.isSQLCipher();
};

export const isLibsql = (): boolean => {
  return OPSQLite.isLibsql();
};

export const isIOSEmbedded = (): boolean => {
  if (Platform.OS !== 'ios') {
    return false;
  }

  return OPSQLite.isIOSEmbedded();
};

/**
 * @deprecated Use `isIOSEmbedded` instead. This alias will be removed in a future release.
 */
export const isIOSEmbeeded = isIOSEmbedded;
