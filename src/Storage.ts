import { open } from './functions';
import { type DB } from './types';

type StorageOptions = {
  location?: string;
  encryptionKey?: string;
};

/**
 * Creates a new async-storage api compatible instance.
 * The encryption key is only used when compiled against the SQLCipher version of op-sqlite.
 */
export class Storage {
  private db: DB;

  constructor(options: StorageOptions) {
    this.db = open({ ...options, name: '__opsqlite_storage.sqlite' });
    this.db.executeSync('PRAGMA mmap_size=268435456');
    this.db.executeSync(
      'CREATE TABLE IF NOT EXISTS storage (key TEXT PRIMARY KEY, value TEXT) WITHOUT ROWID'
    );
  }

  async getItem(key: string): Promise<string | undefined> {
    const result = await this.db.execute(
      'SELECT value FROM storage WHERE key = ?',
      [key]
    );

    let value = result.rows[0]?.value;
    if (typeof value !== 'undefined' && typeof value !== 'string') {
      throw new Error('Value must be a string or undefined');
    }
    return value;
  }

  getItemSync(key: string): string | undefined {
    const result = this.db.executeSync(
      'SELECT value FROM storage WHERE key = ?',
      [key]
    );

    let value = result.rows[0]?.value;
    if (typeof value !== 'undefined' && typeof value !== 'string') {
      throw new Error('Value must be a string or undefined');
    }

    return value;
  }

  async setItem(key: string, value: string) {
    await this.db.execute(
      'INSERT OR REPLACE INTO storage (key, value) VALUES (?, ?)',
      [key, value.toString()]
    );
  }

  setItemSync(key: string, value: string) {
    this.db.executeSync(
      'INSERT OR REPLACE INTO storage (key, value) VALUES (?, ?)',
      [key, value.toString()]
    );
  }

  async removeItem(key: string) {
    await this.db.execute('DELETE FROM storage WHERE key = ?', [key]);
  }

  removeItemSync(key: string) {
    this.db.executeSync('DELETE FROM storage WHERE key = ?', [key]);
  }

  async clear() {
    await this.db.execute('DELETE FROM storage');
  }

  clearSync() {
    this.db.executeSync('DELETE FROM storage');
  }

  getAllKeys() {
    return this.db
      .executeSync('SELECT key FROM storage')
      .rows.map((row: any) => row.key);
  }

  /**
   * Deletes the underlying database file.
   */
  delete() {
    this.db.delete();
  }
}
