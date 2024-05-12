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
export const OPSQLite = proxy as ISQLite;

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
  rows?: {
    /** Raw array with all dataset */
    _array: any[];
    /** The lengh of the dataset */
    length: number;
    /** A convenience function to acess the index based the row object
     * @param idx the row index
     * @returns the row structure identified by column names
     */
    item: (idx: number) => any;
  };
  /**
   * Query metadata, avaliable only for select query results
   */
  metadata?: ColumnMetadata[];
};

/**
 * Column metadata
 * Describes some information about columns fetched by the query
 */
export type ColumnMetadata = {
  /** The name used for this column for this resultset */
  name: string;
  /** The declared column type for this column, when fetched directly from a table or a View resulting from a table column. "UNKNOWN" for dynamic values, like function returned ones. */
  type: string;
  /**
   * The index for this column for this resultset*/
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

interface ISQLite {
  open: (options: {
    name: string;
    location?: string;
    encryptionKey?: string;
  }) => void;
  close: (dbName: string) => void;
  delete: (dbName: string, location?: string) => void;
  attach: (
    mainDbName: string,
    dbNameToAttach: string,
    alias: string,
    location?: string
  ) => void;
  detach: (mainDbName: string, alias: string) => void;
  transaction: (
    dbName: string,
    fn: (tx: Transaction) => Promise<void>
  ) => Promise<void>;
  execute: (dbName: string, query: string, params?: any[]) => QueryResult;
  executeAsync: (
    dbName: string,
    query: string,
    params?: any[]
  ) => Promise<QueryResult>;
  executeBatch: (dbName: string, commands: SQLBatchTuple[]) => BatchQueryResult;
  executeBatchAsync: (
    dbName: string,
    commands: SQLBatchTuple[]
  ) => Promise<BatchQueryResult>;
  loadFile: (dbName: string, location: string) => Promise<FileLoadResult>;
  updateHook: (
    dbName: string,
    callback?:
      | ((params: {
          table: string;
          operation: UpdateHookOperation;
          row?: any;
          rowId: number;
        }) => void)
      | null
  ) => void;
  commitHook: (dbName: string, callback?: (() => void) | null) => void;
  rollbackHook: (dbName: string, callback?: (() => void) | null) => void;
  prepareStatement: (dbName: string, query: string) => PreparedStatementObj;
  loadExtension: (dbName: string, path: string, entryPoint?: string) => void;
  executeRawAsync: (
    dbName: string,
    query: string,
    params?: any[]
  ) => Promise<any[]>;
  getDbPath: (dbName: string, location?: string) => string;
  isSQLCipher: () => boolean;
}

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
    result.rows.item = (idx: number) => result.rows?._array[idx];
  }
}

const _open = OPSQLite.open;
OPSQLite.open = (options: {
  name: string;
  location?: string;
  encryptionKey?: string;
}) => {
  _open(options);

  locks[options.name] = {
    queue: [],
    inProgress: false,
  };
};

const _close = OPSQLite.close;
OPSQLite.close = (dbName: string) => {
  _close(dbName);
  delete locks[dbName];
};

const _execute = OPSQLite.execute;
OPSQLite.execute = (
  dbName: string,
  query: string,
  params?: any[] | undefined
): QueryResult => {
  const sanitizedParams = params?.map((p) => {
    if (ArrayBuffer.isView(p)) {
      return p.buffer;
    }

    return p;
  });

  const result = _execute(dbName, query, sanitizedParams);
  enhanceQueryResult(result);
  return result;
};

const _executeAsync = OPSQLite.executeAsync;
OPSQLite.executeAsync = async (
  dbName: string,
  query: string,
  params?: any[] | undefined
): Promise<QueryResult> => {
  const sanitizedParams = params?.map((p) => {
    if (ArrayBuffer.isView(p)) {
      return p.buffer;
    }

    return p;
  });

  const res = await _executeAsync(dbName, query, sanitizedParams);
  enhanceQueryResult(res);
  return res;
};

const _prepareStatement = OPSQLite.prepareStatement;
OPSQLite.prepareStatement = (dbName: string, query: string) => {
  const stmt = _prepareStatement(dbName, query);

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
};

OPSQLite.transaction = async (
  dbName: string,
  fn: (tx: Transaction) => Promise<void>
): Promise<void> => {
  if (!locks[dbName]) {
    throw Error(`SQLite Error: No lock found on db: ${dbName}`);
  }

  let isFinalized = false;

  // Local transaction context object implementation
  const execute = (query: string, params?: any[]): QueryResult => {
    if (isFinalized) {
      throw Error(
        `SQLite Error: Cannot execute query on finalized transaction: ${dbName}`
      );
    }
    return OPSQLite.execute(dbName, query, params);
  };

  const executeAsync = (query: string, params?: any[] | undefined) => {
    if (isFinalized) {
      throw Error(
        `SQLite Error: Cannot execute query on finalized transaction: ${dbName}`
      );
    }
    return OPSQLite.executeAsync(dbName, query, params);
  };

  const commit = () => {
    if (isFinalized) {
      throw Error(
        `SQLite Error: Cannot execute commit on finalized transaction: ${dbName}`
      );
    }
    const result = OPSQLite.execute(dbName, 'COMMIT');
    isFinalized = true;
    return result;
  };

  const rollback = () => {
    if (isFinalized) {
      throw Error(
        `SQLite Error: Cannot execute rollback on finalized transaction: ${dbName}`
      );
    }
    const result = OPSQLite.execute(dbName, 'ROLLBACK');
    isFinalized = true;
    return result;
  };

  async function run() {
    try {
      await OPSQLite.executeAsync(dbName, 'BEGIN TRANSACTION');

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
      if (!isFinalized) {
        try {
          rollback();
        } catch (rollbackError) {
          throw rollbackError;
        }
      }

      throw executionError;
    } finally {
      locks[dbName]!.inProgress = false;
      isFinalized = false;
      startNextTransaction(dbName);
    }
  }

  return await new Promise((resolve, reject) => {
    const tx: PendingTransaction = {
      start: () => {
        run().then(resolve).catch(reject);
      },
    };

    locks[dbName]!.queue.push(tx);
    startNextTransaction(dbName);
  });
};

const startNextTransaction = (dbName: string) => {
  if (!locks[dbName]) {
    throw Error(`Lock not found for db: ${dbName}`);
  }

  if (locks[dbName]!.inProgress) {
    // Transaction is already in process bail out
    return;
  }

  if (locks[dbName]!.queue.length) {
    locks[dbName]!.inProgress = true;
    const tx = locks[dbName]!.queue.shift();

    if (!tx) {
      throw new Error('Could not get a operation on datebase');
    }

    setImmediate(() => {
      tx.start();
    });
  }
};

export type OPSQLiteConnection = {
  close: () => void;
  delete: () => void;
  attach: (dbNameToAttach: string, alias: string, location?: string) => void;
  detach: (alias: string) => void;
  transaction: (fn: (tx: Transaction) => Promise<void>) => Promise<void>;
  execute: (query: string, params?: any[]) => QueryResult;
  executeAsync: (query: string, params?: any[]) => Promise<QueryResult>;
  executeBatch: (commands: SQLBatchTuple[]) => BatchQueryResult;
  executeBatchAsync: (commands: SQLBatchTuple[]) => Promise<BatchQueryResult>;
  loadFile: (location: string) => Promise<FileLoadResult>;
  updateHook: (
    callback:
      | ((params: {
          table: string;
          operation: UpdateHookOperation;
          row?: any;
          rowId: number;
        }) => void)
      | null
  ) => void;
  commitHook: (callback: (() => void) | null) => void;
  rollbackHook: (callback: (() => void) | null) => void;
  prepareStatement: (query: string) => PreparedStatementObj;
  loadExtension: (path: string, entryPoint?: string) => void;
  executeRawAsync: (query: string, params?: any[]) => Promise<any[]>;
  getDbPath: () => string;
};

export const open = (options: {
  name: string;
  location?: string;
  encryptionKey?: string;
}): OPSQLiteConnection => {
  OPSQLite.open(options);

  return {
    close: () => OPSQLite.close(options.name),
    delete: () => OPSQLite.delete(options.name, options.location),
    attach: (dbNameToAttach: string, alias: string, location?: string) =>
      OPSQLite.attach(options.name, dbNameToAttach, alias, location),
    detach: (alias: string) => OPSQLite.detach(options.name, alias),
    transaction: (fn: (tx: Transaction) => Promise<void>) =>
      OPSQLite.transaction(options.name, fn),
    execute: (query: string, params?: any[] | undefined): QueryResult =>
      OPSQLite.execute(options.name, query, params),
    executeAsync: (
      query: string,
      params?: any[] | undefined
    ): Promise<QueryResult> =>
      OPSQLite.executeAsync(options.name, query, params),
    executeBatch: (commands: SQLBatchTuple[]) =>
      OPSQLite.executeBatch(options.name, commands),
    executeBatchAsync: (commands: SQLBatchTuple[]) =>
      OPSQLite.executeBatchAsync(options.name, commands),
    loadFile: (location: string) => OPSQLite.loadFile(options.name, location),
    updateHook: (callback) => OPSQLite.updateHook(options.name, callback),
    commitHook: (callback) => OPSQLite.commitHook(options.name, callback),
    rollbackHook: (callback) => OPSQLite.rollbackHook(options.name, callback),
    prepareStatement: (query) => OPSQLite.prepareStatement(options.name, query),
    loadExtension: (path, entryPoint) =>
      OPSQLite.loadExtension(options.name, path, entryPoint),
    executeRawAsync: (query, params) =>
      OPSQLite.executeRawAsync(options.name, query, params),
    getDbPath: () => OPSQLite.getDbPath(options.name, options.location),
  };
};

export const moveAssetsDatabase = (args: {
  filename: string;
  path?: string;
  overwrite?: boolean;
}): boolean => {
  return NativeModules.OPSQLite.moveAssetsDatabase(args);
};

export const isSQLCipher = (): boolean => {
  return OPSQLite.isSQLCipher();
};
