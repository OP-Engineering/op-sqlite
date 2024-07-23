// import NativeOPSQLite from './NativeOPSQLite';
import { NativeModules } from 'react-native';

declare global {
  function nativeCallSyncHook(): unknown;
  var __OPSQLiteProxy: object | undefined;
}

if (global.__OPSQLiteProxy == null) {
  if (NativeModules.OPSQLite == null) {
    throw new Error('Base module not found. Maybe try rebuilding the app.');
  }

  if (NativeModules.OPSQLite.install == null) {
    throw new Error(
      'Failed to install op-sqlite: React Native is not running on-device. OPSQLite can only be used when synchronous method invocations (JSI) are possible. If you are using a remote debugger (e.g. Chrome), switch to an on-device debugger (e.g. Flipper) instead.'
    );
  }

  // Call the synchronous blocking install() function
  const result = NativeModules.OPSQLite.install();
  if (result !== true) {
    throw new Error(
      `Failed to install op-sqlite: The native OPSQLite Module could not be installed! Looks like something went wrong when installing JSI bindings, check the native logs for more info`
    );
  }

  // Check again if the constructor now exists. If not, throw an error.
  if (global.__OPSQLiteProxy == null) {
    throw new Error(
      'Failed to install op-sqlite, the native initializer function does not exist. Are you trying to use OPSQLite from different JS Runtimes?'
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

/**
 * Object returned by SQL Query executions {
 *  insertId: Represent the auto-generated row id if applicable
 *  rowsAffected: Number of affected rows if result of a update query
 *  message: if status === 1, here you will find error description
 *  rows: if status is undefined or 0 this object will contain the query results
 * }
 *
 * @interface QueryResult
 */
export type QueryResult = {
  insertId?: number;
  rowsAffected: number;
  res?: any[];
  rows?: {
    /** Raw array with all dataset */
    _array: any[];
    /** The length of the dataset */
    length: number;
    /** A convenience function to access the index based the row object
     * @param idx the row index
     * @returns the row structure identified by column names
     */
    item: (idx: number) => any;
  };
  /**
   * Query metadata, available only for select query results
   */
  metadata?: ColumnMetadata[];
};

/**
 * Column metadata
 * Describes some information about columns fetched by the query
 */
export type ColumnMetadata = {
  /** The name used for this column for this result set */
  name: string;
  /** The declared column type for this column, when fetched directly from a table or a View resulting from a table column. "UNKNOWN" for dynamic values, like function returned ones. */
  type: string;
  /**
   * The index for this column for this result set*/
  index: number;
};

/**
 * Allows the execution of bulk of sql commands
 * inside a transaction
 * If a single query must be executed many times with different arguments, its preferred
 * to declare it a single time, and use an array of array parameters.
 */
export type SQLBatchTuple = [string] | [string, Array<any> | Array<Array<any>>];

export type UpdateHookOperation = 'INSERT' | 'DELETE' | 'UPDATE';

/**
 * status: 0 or undefined for correct execution, 1 for error
 * message: if status === 1, here you will find error description
 * rowsAffected: Number of affected rows if status == 0
 */
export type BatchQueryResult = {
  rowsAffected?: number;
};

/**
 * Result of loading a file and executing every line as a SQL command
 * Similar to BatchQueryResult
 */
export interface FileLoadResult extends BatchQueryResult {
  commands?: number;
}

export interface Transaction {
  commit: () => QueryResult;
  execute: (query: string, params?: any[]) => QueryResult;
  executeAsync: (
    query: string,
    params?: any[] | undefined
  ) => Promise<QueryResult>;
  rollback: () => QueryResult;
}

export interface PendingTransaction {
  /*
   * The start function should not throw or return a promise because the
   * queue just calls it and does not monitor for failures or completions.
   *
   * It should catch any errors and call the resolve or reject of the wrapping
   * promise when complete.
   *
   * It should also automatically commit or rollback the transaction if needed
   */
  start: () => void;
}

export type PreparedStatementObj = {
  bind: (params: any[]) => void;
  execute: () => QueryResult;
};

export type DB = {
  close: () => void;
  delete: (location?: string) => void;
  attach: (
    mainDbName: string,
    dbNameToAttach: string,
    alias: string,
    location?: string
  ) => void;
  detach: (mainDbName: string, alias: string) => void;
  transaction: (fn: (tx: Transaction) => Promise<void>) => Promise<void>;
  execute: (query: string, params?: any[]) => QueryResult;
  executeAsync: (query: string, params?: any[]) => Promise<QueryResult>;
  executeBatch: (commands: SQLBatchTuple[]) => BatchQueryResult;
  executeBatchAsync: (commands: SQLBatchTuple[]) => Promise<BatchQueryResult>;
  loadFile: (location: string) => Promise<FileLoadResult>;
  updateHook: (
    callback?:
      | ((params: {
          table: string;
          operation: UpdateHookOperation;
          row?: any;
          rowId: number;
        }) => void)
      | null
  ) => void;
  commitHook: (callback?: (() => void) | null) => void;
  rollbackHook: (callback?: (() => void) | null) => void;
  prepareStatement: (query: string) => PreparedStatementObj;
  loadExtension: (path: string, entryPoint?: string) => void;
  executeRawAsync: (query: string, params?: any[]) => Promise<any[]>;
  getDbPath: (location?: string) => string;
  reactiveExecute: (params: {
    query: string;
    arguments: any[];
    fireOn: {
      table: string;
      ids?: number[];
    }[];
    callback: (response: any) => void;
  }) => () => void;
  sync: () => void;
};

type OPSQLiteProxy = {
  open: (options: {
    name: string;
    location?: string;
    encryptionKey?: string;
  }) => DB;
  openRemote: (options: { url: string; authToken: string }) => DB;
  openSync: (options: {
    url: string;
    authToken: string;
    name: string;
    location?: string;
  }) => DB;
  isSQLCipher: () => boolean;
  isLibsql: () => boolean;
};

const locks: Record<
  string,
  { queue: PendingTransaction[]; inProgress: boolean }
> = {};

// Enhance some host functions
// Add 'item' function to result object to allow the sqlite-storage typeorm driver to work
function enhanceQueryResult(result: QueryResult): void {
  // Add 'item' function to result object to allow the sqlite-storage typeorm driver to work
  if (result.rows == null) {
    result.rows = {
      _array: [],
      length: 0,
      item: (idx: number) => result.rows?._array[idx],
    };
  } else {
    result.res = result.rows._array;
    result.rows.item = (idx: number) => result.rows?._array[idx];
  }
}

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
    executeBatchAsync: db.executeBatchAsync,

    loadFile: db.loadFile,
    updateHook: db.updateHook,
    commitHook: db.commitHook,
    rollbackHook: db.rollbackHook,
    loadExtension: db.loadExtension,
    executeRawAsync: db.executeRawAsync,
    getDbPath: db.getDbPath,
    reactiveExecute: db.reactiveExecute,
    sync: db.sync,
    close: () => {
      db.close();
      delete locks[options.url];
    },
    execute: (query: string, params?: any[] | undefined): QueryResult => {
      const sanitizedParams = params?.map((p) => {
        if (ArrayBuffer.isView(p)) {
          return p.buffer;
        }

        return p;
      });

      const result = db.execute(query, sanitizedParams);
      enhanceQueryResult(result);
      return result;
    },
    executeAsync: async (
      query: string,
      params?: any[] | undefined
    ): Promise<QueryResult> => {
      const sanitizedParams = params?.map((p) => {
        if (ArrayBuffer.isView(p)) {
          return p.buffer;
        }

        return p;
      });

      const result = await db.executeAsync(query, sanitizedParams);
      enhanceQueryResult(result);
      return result;
    },
    prepareStatement: (query: string) => {
      const stmt = db.prepareStatement(query);

      return {
        bind: (params: any[]) => {
          const sanitizedParams = params.map((p) => {
            if (ArrayBuffer.isView(p)) {
              return p.buffer;
            }

            return p;
          });

          stmt.bind(sanitizedParams);
        },
        execute: () => {
          const res = stmt.execute();
          enhanceQueryResult(res);
          return res;
        },
      };
    },
    transaction: async (
      fn: (tx: Transaction) => Promise<void>
    ): Promise<void> => {
      let isFinalized = false;

      // Local transaction context object implementation
      const execute = (query: string, params?: any[]): QueryResult => {
        if (isFinalized) {
          throw Error(
            `OP-Sqlite Error: Database: ${options.url}. Cannot execute query on finalized transaction`
          );
        }
        return enhancedDb.execute(query, params);
      };

      const executeAsync = (query: string, params?: any[] | undefined) => {
        if (isFinalized) {
          throw Error(
            `OP-Sqlite Error: Database: ${options.url}. Cannot execute query on finalized transaction`
          );
        }
        return enhancedDb.executeAsync(query, params);
      };

      const commit = () => {
        if (isFinalized) {
          throw Error(
            `OP-Sqlite Error: Database: ${options.url}. Cannot execute query on finalized transaction`
          );
        }
        const result = enhancedDb.execute('COMMIT;');
        isFinalized = true;
        return result;
      };

      const rollback = () => {
        if (isFinalized) {
          throw Error(
            `OP-Sqlite Error: Database: ${options.url}. Cannot execute query on finalized transaction`
          );
        }
        const result = enhancedDb.execute('ROLLBACK;');
        isFinalized = true;
        return result;
      };

      async function run() {
        try {
          await enhancedDb.executeAsync('BEGIN TRANSACTION;');

          await fn({
            commit,
            execute,
            executeAsync,
            rollback,
          });

          if (!isFinalized) {
            commit();
          }
        } catch (executionError) {
          console.warn('transaction error', executionError);
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

export const openSync = (options: {
  url: string;
  authToken: string;
  name: string;
  location?: string;
  syncInterval: number;
}): DB => {
  if (!isLibsql()) {
    throw new Error('This function is only available for libsql');
  }

  const db = OPSQLite.openSync(options);
  const enhancedDb = enhanceDB(db, options);

  return enhancedDb;
};

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

export const isSQLCipher = (): boolean => {
  return OPSQLite.isSQLCipher();
};

export const isLibsql = (): boolean => {
  return OPSQLite.isLibsql();
};
