type Scalar = string | number | boolean | null | ArrayBuffer | ArrayBufferView;

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
type QueryResult = {
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
type ColumnMetadata = {
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
type SQLBatchTuple = [string] | [string, Array<any> | Array<Array<any>>];

type UpdateHookOperation = 'INSERT' | 'DELETE' | 'UPDATE';

/**
 * status: 0 or undefined for correct execution, 1 for error
 * message: if status === 1, here you will find error description
 * rowsAffected: Number of affected rows if status == 0
 */
type BatchQueryResult = {
  rowsAffected?: number;
};

/**
 * Result of loading a file and executing every line as a SQL command
 * Similar to BatchQueryResult
 */
type FileLoadResult = BatchQueryResult & {
  commands?: number;
};

type Transaction = {
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

type PreparedStatement = {
  bind: (params: any[]) => Promise<void>;
  execute: () => Promise<QueryResult>;
};

type DB = {
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
  isIOSEmbedded: () => boolean;
};
