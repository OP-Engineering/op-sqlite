import * as fs from 'node:fs';
import * as path from 'node:path';
import Database from 'better-sqlite3';
import type {
    BatchQueryResult,
    ColumnMetadata,
    DB,
    FileLoadResult,
    PreparedStatement,
    QueryResult,
    Scalar,
    SQLBatchTuple,
    Transaction,
    UpdateHookOperation,
} from './types';

export class NodeDatabase implements DB {
  private db: Database.Database;
  private dbPath: string;
  private updateHookCallback?: ((params: {
    table: string;
    operation: UpdateHookOperation;
    row?: any;
    rowId: number;
  }) => void) | null;
  private commitHookCallback?: (() => void) | null;
  private rollbackHookCallback?: (() => void) | null;

  constructor(name: string, location?: string) {
    const dbLocation = location || './';
    this.dbPath = path.join(dbLocation, name);

    // Ensure directory exists
    const dir = path.dirname(this.dbPath);
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir, { recursive: true });
    }

    this.db = new Database(this.dbPath);
    this.setupHooks();
  }

  private setupHooks(): void {
    // Setup update hook if needed
    // if (this.db.function) {
      // Note: better-sqlite3 doesn't have direct update hook support
      // This is a limitation compared to the native implementation
    // }
  }

  private convertParams(params?: Scalar[]): any[] | undefined {
    if (!params) return undefined;

    return params.map(param => {
      if (param instanceof ArrayBuffer) {
        return Buffer.from(param);
      } else if (ArrayBuffer.isView(param)) {
        return Buffer.from(param.buffer, param.byteOffset, param.byteLength);
      }
      return param;
    });
  }

  private convertRows(stmt: Database.Statement, rows: any[]): QueryResult {
    const columnNames = stmt.columns().map(col => col.name);
    const metadata: ColumnMetadata[] = stmt.columns().map((col, index) => ({
      name: col.name,
      type: col.type || 'UNKNOWN',
      index,
    }));

    return {
      rowsAffected: 0,
      rows: rows as Array<Record<string, Scalar>>,
      columnNames,
      metadata,
    };
  }

  executeSync(query: string, params?: Scalar[]): QueryResult {
    try {
      const convertedParams = this.convertParams(params);
      const stmt = this.db.prepare(query);

      // Check if it's a SELECT query
      const isSelect = query.trim().toUpperCase().startsWith('SELECT');

      if (isSelect) {
        const rows = convertedParams
          ? stmt.all(...convertedParams)
          : stmt.all();
        return this.convertRows(stmt, rows);
      } else {
        const info = convertedParams
          ? stmt.run(...convertedParams)
          : stmt.run();

        return {
          rowsAffected: info.changes,
          insertId: info.lastInsertRowid as number,
          rows: [],
        };
      }
    } catch (error: any) {
      throw new Error(`SQL Error: ${error.message}`);
    }
  }

  async execute(query: string, params?: Scalar[]): Promise<QueryResult> {
    return Promise.resolve(this.executeSync(query, params));
  }

  async executeWithHostObjects(query: string, params?: Scalar[]): Promise<QueryResult> {
    // For Node.js, this is the same as execute since we don't have HostObjects
    return this.execute(query, params);
  }

  executeRawSync(query: string, params?: Scalar[]): any[] {
    try {
      const convertedParams = this.convertParams(params);
      const stmt = this.db.prepare(query);
      const rows = convertedParams
        ? stmt.raw().all(...convertedParams)
        : stmt.raw().all();
      return rows;
    } catch (error: any) {
      throw new Error(`SQL Error: ${error.message}`);
    }
  }

  async executeRaw(query: string, params?: Scalar[]): Promise<any[]> {
    return Promise.resolve(this.executeRawSync(query, params));
  }

  async executeBatch(commands: SQLBatchTuple[]): Promise<BatchQueryResult> {
    let totalRowsAffected = 0;

    const transaction = this.db.transaction(() => {
      for (const command of commands) {
        const [query, params] = command;

        if (Array.isArray(params) && params.length > 0 && Array.isArray(params[0])) {
          // Multiple parameter sets
          const stmt = this.db.prepare(query);
          for (const paramSet of params as Scalar[][]) {
            const converted = this.convertParams(paramSet);
            const info = converted ? stmt.run(...converted) : stmt.run();
            totalRowsAffected += info.changes;
          }
        } else {
          // Single parameter set or no parameters
          const converted = this.convertParams(params as Scalar[] | undefined);
          const stmt = this.db.prepare(query);
          const info = converted ? stmt.run(...converted) : stmt.run();
          totalRowsAffected += info.changes;
        }
      }
    });

    transaction();

    return { rowsAffected: totalRowsAffected };
  }

  async loadFile(location: string): Promise<FileLoadResult> {
    const fileContent = fs.readFileSync(location, 'utf-8');
    const statements = fileContent
      .split(';')
      .map(s => s.trim())
      .filter(s => s.length > 0);

    let totalRowsAffected = 0;
    let commandCount = 0;

    const transaction = this.db.transaction(() => {
      for (const statement of statements) {
        const info = this.db.prepare(statement).run();
        totalRowsAffected += info.changes;
        commandCount++;
      }
    });

    transaction();

    return {
      rowsAffected: totalRowsAffected,
      commands: commandCount,
    };
  }

  async transaction(fn: (tx: Transaction) => Promise<void>): Promise<void> {
    const transaction: Transaction = {
      execute: async (query: string, params?: Scalar[]) => {
        return this.execute(query, params);
      },
      commit: async () => {
        return { rowsAffected: 0, rows: [] };
      },
      rollback: () => {
        throw new Error('ROLLBACK');
      },
    };

    // Manually control transaction with BEGIN/COMMIT/ROLLBACK to support async operations
    this.executeSync('BEGIN TRANSACTION');

    try {
      await fn(transaction);

      this.executeSync('COMMIT');

      if (this.commitHookCallback) {
        this.commitHookCallback();
      }
    } catch (error: any) {
      this.executeSync('ROLLBACK');

      if (this.rollbackHookCallback) {
        this.rollbackHookCallback();
      }

      throw error;
    }
  }

  prepareStatement(query: string): PreparedStatement {
    const stmt = this.db.prepare(query);
    let boundParams: any[] = [];

    return {
      bindSync: (params: any[]) => {
        boundParams = this.convertParams(params) || [];
      },
      bind: async (params: any[]) => {
        boundParams = this.convertParams(params) || [];
      },
      execute: async () => {
        const isSelect = query.trim().toUpperCase().startsWith('SELECT');

        if (isSelect) {
          const rows = boundParams.length > 0
            ? stmt.all(...boundParams)
            : stmt.all();
          return this.convertRows(stmt, rows);
        } else {
          const info = boundParams.length > 0
            ? stmt.run(...boundParams)
            : stmt.run();

          return {
            rowsAffected: info.changes,
            insertId: info.lastInsertRowid as number,
            rows: [],
          };
        }
      },
    };
  }

  attach(params: {
    secondaryDbFileName: string;
    alias: string;
    location?: string;
  }): void {
    const dbLocation = params.location || './';
    const dbPath = path.join(dbLocation, params.secondaryDbFileName);
    this.db.prepare(`ATTACH DATABASE ? AS ?`).run(dbPath, params.alias);
  }

  detach(alias: string): void {
    this.db.prepare(`DETACH DATABASE ?`).run(alias);
  }

  updateHook(
    callback?:
      | ((params: {
          table: string;
          operation: UpdateHookOperation;
          row?: any;
          rowId: number;
        }) => void)
      | null
  ): void {
    this.updateHookCallback = callback;
    // Note: better-sqlite3 doesn't support update hooks directly
    console.warn('Update hooks are not fully supported in the Node.js implementation');
  }

  commitHook(callback?: (() => void) | null): void {
    this.commitHookCallback = callback;
  }

  rollbackHook(callback?: (() => void) | null): void {
    this.rollbackHookCallback = callback;
  }

  loadExtension(path: string, entryPoint?: string): void {
    // better-sqlite3 supports loadExtension but it may not be enabled by default
    try {
      if (entryPoint) {
        (this.db as any).loadExtension(path, entryPoint);
      } else {
        (this.db as any).loadExtension(path);
      }
    } catch (error: any) {
      throw new Error(`Failed to load extension: ${error.message}`);
    }
  }

  getDbPath(location?: string): string {
    return this.dbPath;
  }

  reactiveExecute(params: {
    query: string;
    arguments: any[];
    fireOn: { table: string; ids?: number[] }[];
    callback: (response: any) => void;
  }): () => void {
    // Reactive queries are not supported in Node.js implementation
    console.warn('Reactive queries are not supported in the Node.js implementation');
    return () => {};
  }

  sync(): void {
    // LibSQL sync is not supported in the Node.js implementation
    throw new Error('sync() is only available with libsql');
  }

  setReservedBytes(reservedBytes: number): void {
    // SQLCipher specific, not supported in standard SQLite
    console.warn('setReservedBytes is not supported in the Node.js implementation');
  }

  getReservedBytes(): number {
    // SQLCipher specific, not supported in standard SQLite
    return 0;
  }

  async flushPendingReactiveQueries(): Promise<void> {
    // No-op for Node.js implementation
  }

  close(): void {
    if (this.db && this.db.open) {
      this.db.close();
    }
  }

  delete(location?: string): void {
    this.close();
    const dbLocation = location || './';
    const dbPath = path.join(dbLocation, path.basename(this.dbPath));

    if (fs.existsSync(dbPath)) {
      fs.unlinkSync(dbPath);
    }
  }
}
