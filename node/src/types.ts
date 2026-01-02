// Re-export all types from the main package
export type Scalar =
  | string
  | number
  | boolean
  | null
  | ArrayBuffer
  | ArrayBufferView;

export type QueryResult = {
  insertId?: number;
  rowsAffected: number;
  res?: any[];
  rows: Array<Record<string, Scalar>>;
  rawRows?: Scalar[][];
  columnNames?: string[];
  metadata?: ColumnMetadata[];
};

export type ColumnMetadata = {
  name: string;
  type: string;
  index: number;
};

export type SQLBatchTuple =
  | [string]
  | [string, Scalar[]]
  | [string, Scalar[][]];

export type UpdateHookOperation = 'INSERT' | 'DELETE' | 'UPDATE';

export type BatchQueryResult = {
  rowsAffected?: number;
};

export type FileLoadResult = BatchQueryResult & {
  commands?: number;
};

export type Transaction = {
  commit: () => Promise<QueryResult>;
  execute: (query: string, params?: Scalar[]) => Promise<QueryResult>;
  rollback: () => QueryResult;
};

export type PreparedStatement = {
  bind: (params: any[]) => Promise<void>;
  bindSync: (params: any[]) => void;
  execute: () => Promise<QueryResult>;
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
  setReservedBytes: (reservedBytes: number) => void;
  getReservedBytes: () => number;
  flushPendingReactiveQueries: () => Promise<void>;
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
  }) => DB;
  openV2: (options: { path: string; encryptionKey?: string }) => DB;
  openRemote: (options: { url: string; authToken: string }) => DB;
  openSync: (options: DBParams) => DB;
  isSQLCipher: () => boolean;
  isLibsql: () => boolean;
  isIOSEmbedded: () => boolean;
};
