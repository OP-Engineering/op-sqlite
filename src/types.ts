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
  | [string, Scalar[]]
  | [string, Scalar[][]];

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
  rollback: () => QueryResult;
};

export type _PendingTransaction = {
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

export type _InternalDB = {
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
  executeRawSync: (query: string, params?: Scalar[]) => any[];
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
   * Same as `executeRaw` but it will block the JS thread and therefore your UI and should be used with caution
   * It will return an array of arrays with just the values and not the keys
   */
  executeRawSync: (query: string, params?: Scalar[]) => any[];
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
  }) => _InternalDB;
  openRemote: (options: { url: string; authToken: string }) => _InternalDB;
  openSync: (options: DBParams) => _InternalDB;
  isSQLCipher: () => boolean;
  isLibsql: () => boolean;
  isIOSEmbedded: () => boolean;
};
