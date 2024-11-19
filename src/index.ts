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
  rows: Array<Record<string, string | number | boolean | ArrayBufferLike>>;
  // An array of intermediate results, just values without column names
  rawRows?: any[];
  columnNames?: string[];
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
  commit: () => Promise<QueryResult>;
  execute: (query: string, params?: any[]) => Promise<QueryResult>;
  rollback: () => Promise<QueryResult>;
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
  execute: () => Promise<QueryResult>;
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
  execute: (query: string, params?: any[]) => Promise<QueryResult>;
  executeSync: (query: string, params?: any[]) => QueryResult;
  executeWithHostObjects: (
    query: string,
    params?: any[]
  ) => Promise<QueryResult>;
  executeBatch: (commands: SQLBatchTuple[]) => Promise<BatchQueryResult>;
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
  executeRaw: (query: string, params?: any[]) => Promise<any[]>;
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
  /** This function is only available for libsql.
   * Allows to trigger a sync the database with it's remote replica
   * In order for this function to work you need to use openSync or openRemote functions
   * with libsql: true in the package.json
   *
   * The database is hosted in turso
   **/
  sync: () => void;
  flushPendingReactiveQueries: () => Promise<void>;
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
    syncInterval?: number;
  }) => DB;
  isSQLCipher: () => boolean;
  isLibsql: () => boolean;
};

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
      params?: any[] | undefined
    ): Promise<QueryResult> => {
      const sanitizedParams = params?.map((p) => {
        if (ArrayBuffer.isView(p)) {
          return p.buffer;
        }

        return p;
      });

      const result = await db.executeWithHostObjects(query, sanitizedParams);

      return result;
    },
    execute: async (
      query: string,
      params?: any[] | undefined
    ): Promise<QueryResult> => {
      const sanitizedParams = params?.map((p) => {
        if (ArrayBuffer.isView(p)) {
          return p.buffer;
        }

        return p;
      });

      let intermediateResult = await db.execute(query, sanitizedParams);

      let rows: any[] = [];
      for (let i = 0; i < (intermediateResult.rawRows?.length ?? 0); i++) {
        let row: any = {};
        for (let j = 0; j < intermediateResult.columnNames!.length; j++) {
          let columnName = intermediateResult.columnNames![j]!;
          let value = intermediateResult.rawRows![i][j];

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
    executeSync:  (
      query: string,
      params?: any[] | undefined
    ): QueryResult => {
      const sanitizedParams = params?.map((p) => {
        if (ArrayBuffer.isView(p)) {
          return p.buffer;
        }

        return p;
      });

      let intermediateResult =  db.executeSync(query, sanitizedParams);

      let rows: any[] = [];
      for (let i = 0; i < (intermediateResult.rawRows?.length ?? 0); i++) {
        let row: any = {};
        for (let j = 0; j < intermediateResult.columnNames!.length; j++) {
          let columnName = intermediateResult.columnNames![j]!;
          let value = intermediateResult.rawRows![i][j];

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
        bind: (params: any[]) => {
          const sanitizedParams = params.map((p) => {
            if (ArrayBuffer.isView(p)) {
              return p.buffer;
            }

            return p;
          });

          stmt.bind(sanitizedParams);
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
