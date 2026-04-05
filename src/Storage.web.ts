import { openAsync } from './functions.web';
import { type DB } from './types';

type StorageOptions = {
  location?: string;
  encryptionKey?: string;
};

export class Storage {
  private dbPromise: Promise<DB>;

  constructor(options: StorageOptions) {
    this.dbPromise = (async () => {
      const db = await openAsync({ ...options, name: '__opsqlite_storage.sqlite' });
      await db.execute(
        'CREATE TABLE IF NOT EXISTS storage (key TEXT PRIMARY KEY, value TEXT)'
      );
      return db;
    })();
  }

  private async getDb(): Promise<DB> {
    return this.dbPromise;
  }

  async getItem(key: string): Promise<string | undefined> {
    const db = await this.getDb();
    const result = await db.execute('SELECT value FROM storage WHERE key = ?', [key]);

    const value = result.rows[0]?.value;
    if (typeof value !== 'undefined' && typeof value !== 'string') {
      throw new Error('Value must be a string or undefined');
    }

    return value;
  }

  getItemSync(_key: string): string | undefined {
    throw new Error('[op-sqlite] Storage sync APIs are not supported on web.');
  }

  async setItem(key: string, value: string): Promise<void> {
    const db = await this.getDb();
    await db.execute('INSERT OR REPLACE INTO storage (key, value) VALUES (?, ?)', [
      key,
      value,
    ]);
  }

  setItemSync(_key: string, _value: string): void {
    throw new Error('[op-sqlite] Storage sync APIs are not supported on web.');
  }

  async removeItem(key: string): Promise<void> {
    const db = await this.getDb();
    await db.execute('DELETE FROM storage WHERE key = ?', [key]);
  }

  removeItemSync(_key: string): void {
    throw new Error('[op-sqlite] Storage sync APIs are not supported on web.');
  }

  async clear(): Promise<void> {
    const db = await this.getDb();
    await db.execute('DELETE FROM storage');
  }

  clearSync(): void {
    throw new Error('[op-sqlite] Storage sync APIs are not supported on web.');
  }

  async getAllKeys(): Promise<string[]> {
    const db = await this.getDb();
    const result = await db.execute('SELECT key FROM storage');

    return result.rows.map((row: any) => String(row.key));
  }

  delete(): void {
    throw new Error('[op-sqlite] Storage.delete() is not supported on web.');
  }
}
