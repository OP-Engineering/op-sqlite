import { NativeModules, Platform } from 'react-native';
export { Storage } from './Storage';

export type Scalar =
  | string
  | number
  | boolean
  | null
  | ArrayBuffer
  | ArrayBufferView;

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
  rows: Array<Record<string, Scalar>>;
  // An array of intermediate results, just values without column names
  rawRows?: Scalar[][];
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
export type SQLBatchTuple =
  | [string]
  | [string, Array<Scalar> | Array<Array<Scalar>>];

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
export type FileLoadResult = BatchQueryResult & {
  commands?: number;
};

export type Transaction = {
  commit: () => Promise<QueryResult>;
  execute: (query: string, params?: Scalar[]) => Promise<QueryResult>;
  rollback: () => Promise<QueryResult>;
};

type PendingTransaction = {
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
};

export type PreparedStatement = {
  bind: (params: any[]) => Promise<void>;
  bindSync: (params: any[]) => void;
  execute: () => Promise<QueryResult>;
};

type InternalDB = {
  close: () => void;
  delete: (location?: string) => void;
  attach: (params: {
    secondaryDbFileName: string;
    alias: string;
    location?: string;
  }) => void;
  detach: (alias: string) => void;
  transaction: (fn: (tx: Transaction) => Promise<void>) => Promise<void>;
  executeSync: (query: string, params?: Scalar[]) => QueryResult;
  execute: (query: string, params?: Scalar[]) => Promise<QueryResult>;
  executeWithHostObjects: (
    query: string,
    params?: Scalar[]
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
  prepareStatement: (query: string) => PreparedStatement;
  loadExtension: (path: string, entryPoint?: string) => void;
  executeRaw: (query: string, params?: Scalar[]) => Promise<any[]>;
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
  flushPendingReactiveQueries: () => Promise<void>;
};

export type DB = {
  close: () => void;
  delete: (location?: string) => void;
  attach: (params: {
    secondaryDbFileName: string;
    alias: string;
    location?: string;
  }) => void;
  detach: (alias: string) => void;
  /**
   * Wraps all the executions into a transaction. If an error is thrown it will rollback all of the changes
   *
   * You need to use this if you are using reactive queries for the queries to fire after the transaction is done
   */
  transaction: (fn: (tx: Transaction) => Promise<void>) => Promise<void>;
  /**
   * Sync version of the execute function
   * It will block the JS thread and therefore your UI and should be used with caution
   *
   * When writing your queries, you can use the ? character as a placeholder for parameters
   * The parameters will be automatically escaped and sanitized
   *
   * Example:
   * db.executeSync('SELECT * FROM table WHERE id = ?', [1]);
   *
   * If you are writing a query that doesn't require parameters, you can omit the second argument
   *
   * If you are writing to the database YOU SHOULD BE USING TRANSACTIONS!
   * Transactions protect you from partial writes and ensure that your data is always in a consistent state
   *
   * @param query
   * @param params
   * @returns QueryResult
   */
  executeSync: (query: string, params?: Scalar[]) => QueryResult;
  /**
   * Basic query execution function, it is async don't forget to await it
   *
   * When writing your queries, you can use the ? character as a placeholder for parameters
   * The parameters will be automatically escaped and sanitized
   *
   * Example:
   * await db.execute('SELECT * FROM table WHERE id = ?', [1]);
   *
   * If you are writing a query that doesn't require parameters, you can omit the second argument
   *
   * If you are writing to the database YOU SHOULD BE USING TRANSACTIONS!
   * Transactions protect you from partial writes and ensure that your data is always in a consistent state
   *
   * If you need a large amount of queries ran as fast as possible you should be using `executeBatch`, `executeRaw`, `loadFile` or `executeWithHostObjects`
   *
   * @param query string of your SQL query
   * @param params a list of parameters to bind to the query, if any
   * @returns Promise<QueryResult> with the result of the query
   */
  execute: (query: string, params?: Scalar[]) => Promise<QueryResult>;
  /**
   * Similar to the execute function but returns the response in HostObjects
   * Read more about HostObjects in the documentation and their pitfalls
   *
   * Will be a lot faster than the normal execute functions when returning data but you will pay when accessing the fields
   * as the conversion is done the moment you access any field
   * @param query
   * @param params
   * @returns
   */
  executeWithHostObjects: (
    query: string,
    params?: Scalar[]
  ) => Promise<QueryResult>;
  /**
   * Executes all the queries in the params inside a single transaction
   *
   * It's faster than executing single queries as data is sent to the native side only once
   * @param commands
   * @returns Promise<BatchQueryResult>
   */
  executeBatch: (commands: SQLBatchTuple[]) => Promise<BatchQueryResult>;
  /**
   * Loads a SQLite Dump from disk. It will be the fastest way to execute a large set of queries as no JS is involved
   */
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
  /**
   * Constructs a prepared statement from the query string
   * The statement can be re-bound with parameters and executed
   * The performance gain is significant when the same query is executed multiple times, NOT when the query is executed (once)
   * The cost lies in the preparation of the statement as it is compiled and optimized by the sqlite engine, the params can then rebound
   * but the query itself is already optimized
   *
   * @param query string of your SQL query
   * @returns Prepared statement object
   */
  prepareStatement: (query: string) => PreparedStatement;
  /**
   * Loads a runtime loadable sqlite extension. Libsql and iOS embedded version do not support loading extensions
   */
  loadExtension: (path: string, entryPoint?: string) => void;
  /**
   * Same as `execute` except the results are not returned in objects but rather in arrays with just the values and not the keys
   * It will be faster since a lot of repeated work is skipped and only the values you care about are returned
   */
  executeRaw: (query: string, params?: Scalar[]) => Promise<any[]>;
  /**
   * Get's the absolute path to the db file. Useful for debugging on local builds and for attaching the DB from users devices
   */
  getDbPath: (location?: string) => string;
  /**
   * Reactive execution of queries when data is written to the database. Check the docs for how to use them.
   */
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
};

export type DBParams = {
  url?: string;
  authToken?: string;
  name?: string;
  location?: string;
  syncInterval?: number;
};

export type OPSQLiteProxy = {
  open: (options: {
    name: string;
    location?: string;
    encryptionKey?: string;
  }) => InternalDB;
  openRemote: (options: { url: string; authToken: string }) => InternalDB;
  openSync: (options: DBParams) => InternalDB;
  isSQLCipher: () => boolean;
  isLibsql: () => boolean;
  isIOSEmbedded: () => boolean;
};

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
const OPSQLite = proxy as OPSQLiteProxy;

export const {
  IOS_DOCUMENT_PATH,
  IOS_LIBRARY_PATH,
  ANDROID_DATABASE_PATH,
  ANDROID_FILES_PATH,
  ANDROID_EXTERNAL_FILES_PATH,
} = !!NativeModules.OPSQLite.getConstants
  ? NativeModules.OPSQLite.getConstants()
  : NativeModules.OPSQLite;

function enhanceDB(db: InternalDB, options: DBParams): DB {
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
      const sanitizedCommands = commands.map(([query, params]) => {
        if (params) {
          return [query, sanitizeArrayBuffersInArray(params)];
        }

        return [query];
      });

      return db.executeBatch(sanitizedCommands as any[]);
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
        const result = await enhancedDb.execute('COMMIT;');

        await db.flushPendingReactiveQueries();

        isFinalized = true;
        return result;
      };

      const rollback = async (): Promise<QueryResult> => {
        if (isFinalized) {
          throw Error(
            `OP-Sqlite Error: Database: ${
              options.name || options.url
            }. Cannot execute query on finalized transaction`
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

export const isIOSEmbeeded = (): boolean => {
  if (Platform.OS !== 'ios') {
    return false;
  }

  return OPSQLite.isIOSEmbedded();
};
