import { NativeModules, Platform } from 'react-native';

declare global {
  function nativeCallSyncHook(): unknown;
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

export const {
  IOS_DOCUMENT_PATH,
  IOS_LIBRARY_PATH,
  ANDROID_DATABASE_PATH,
  ANDROID_FILES_PATH,
  ANDROID_EXTERNAL_FILES_PATH,
} = !!NativeModules.OPSQLite.getConstants
  ? NativeModules.OPSQLite.getConstants()
  : NativeModules.OPSQLite;

const locks: Record<
  string,
  { queue: PendingTransaction[]; inProgress: boolean }
> = {};

function enhanceDB(db: DB, options: any): DB {
  const lock = {
    queue: [] as PendingTransaction[],
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

  // spreading the object is not working, so we need to do it manually
  let enhancedDb = {
    delete: db.delete,
    attach: db.attach,
    detach: db.detach,
    executeBatch: db.executeBatch,
    loadFile: db.loadFile,
    updateHook: db.updateHook,
    commitHook: db.commitHook,
    rollbackHook: db.rollbackHook,
    loadExtension: db.loadExtension,
    executeRaw: db.executeRaw,
    getDbPath: db.getDbPath,
    reactiveExecute: db.reactiveExecute,
    sync: db.sync,
    flushPendingReactiveQueries: db.flushPendingReactiveQueries,
    close: () => {
      db.close();
      delete locks[options.url];
    },
    executeWithHostObjects: async (
      query: string,
      params?: Scalar[]
    ): Promise<QueryResult> => {
      const sanitizedParams = params?.map((p) => {
        if (ArrayBuffer.isView(p)) {
          return p.buffer;
        }

        return p;
      });

      return sanitizedParams
        ? await db.executeWithHostObjects(query, sanitizedParams as Scalar[])
        : await db.executeWithHostObjects(query);
    },
    executeSync: (query: string, params?: Scalar[]): QueryResult => {
      const sanitizedParams = params?.map((p) => {
        if (ArrayBuffer.isView(p)) {
          return p.buffer;
        }

        return p;
      });

      let intermediateResult = sanitizedParams
        ? db.executeSync(query, sanitizedParams as Scalar[])
        : db.executeSync(query);

      let rows: any[] = [];
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
    execute: async (
      query: string,
      params?: Scalar[] | undefined
    ): Promise<QueryResult> => {
      const sanitizedParams = params?.map((p) => {
        if (ArrayBuffer.isView(p)) {
          return p.buffer;
        }

        return p;
      });

      let intermediateResult = await db.execute(
        query,
        sanitizedParams as Scalar[]
      );

      let rows: any[] = [];
      for (let i = 0; i < (intermediateResult.rawRows?.length ?? 0); i++) {
        let row: any = {};
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
        bind: async (params: any[]) => {
          const sanitizedParams = params.map((p) => {
            if (ArrayBuffer.isView(p)) {
              return p.buffer;
            }

            return p;
          });

          await stmt.bind(sanitizedParams);
        },
        execute: stmt.execute,
      };
    },
    transaction: async (
      fn: (tx: Transaction) => Promise<void>
    ): Promise<void> => {
      let isFinalized = false;

      const execute = async (query: string, params?: any[] | undefined) => {
        if (isFinalized) {
          throw Error(
            `OP-Sqlite Error: Database: ${options.url}. Cannot execute query on finalized transaction`
          );
        }
        return await enhancedDb.execute(query, params);
      };

      const commit = async (): Promise<QueryResult> => {
        if (isFinalized) {
          throw Error(
            `OP-Sqlite Error: Database: ${options.url}. Cannot execute query on finalized transaction`
          );
        }
        const result = await enhancedDb.execute('COMMIT;');

        await enhancedDb.flushPendingReactiveQueries();

        isFinalized = true;
        return result;
      };

      const rollback = async (): Promise<QueryResult> => {
        if (isFinalized) {
          throw Error(
            `OP-Sqlite Error: Database: ${options.url}. Cannot execute query on finalized transaction`
          );
        }
        const result = await enhancedDb.execute('ROLLBACK;');
        isFinalized = true;
        return result;
      };

      async function run() {
        try {
          await enhancedDb.execute('BEGIN TRANSACTION;');

          await fn({
            commit,
            execute,
            rollback,
          });

          if (!isFinalized) {
            await commit();
          }
        } catch (executionError) {
          // console.warn('transaction error', executionError);
          if (!isFinalized) {
            try {
              await rollback();
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
        const tx: PendingTransaction = {
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

/** Open a replicating connection via libsql to a turso db
 * libsql needs to be enabled on your package.json
 */
export const openSync = (options: {
  url: string;
  authToken: string;
  name: string;
  location?: string;
  syncInterval?: number;
}): DB => {
  if (!isLibsql()) {
    throw new Error('This function is only available for libsql');
  }

  const db = OPSQLite.openSync(options);
  const enhancedDb = enhanceDB(db, options);

  return enhancedDb;
};

/** Open a remote connection via libsql to a turso db
 * libsql needs to be enabled on your package.json
 */
export const openRemote = (options: { url: string; authToken: string }): DB => {
  if (!isLibsql()) {
    throw new Error('This function is only available for libsql');
  }

  const db = OPSQLite.openRemote(options);
  const enhancedDb = enhanceDB(db, options);

  return enhancedDb;
};

export const open = (options: {
  name: string;
  location?: string;
  encryptionKey?: string;
}): DB => {
  if (options.location?.startsWith('file://')) {
    console.warn(
      "[op-sqlite] You are passing a path with 'file://' prefix, it's automatically removed"
    );
    options.location = options.location.substring(7);
  }

  const db = OPSQLite.open(options);
  const enhancedDb = enhanceDB(db, options);

  return enhancedDb;
};

export const moveAssetsDatabase = async (args: {
  filename: string;
  path?: string;
  overwrite?: boolean;
}): Promise<boolean> => {
  return NativeModules.OPSQLite.moveAssetsDatabase(args);
};

export const getDylibPath = (bundle: string, name: string): string => {
  return NativeModules.OPSQLite.getDylibPath(bundle, name);
};

export const isSQLCipher = (): boolean => {
  return OPSQLite.isSQLCipher();
};

export const isLibsql = (): boolean => {
  return OPSQLite.isLibsql();
};

export const isIOSEmbeeded = (): boolean => {
  if (Platform.OS !== 'ios') {
    return false;
  }

  return OPSQLite.isIOSEmbedded();
};
