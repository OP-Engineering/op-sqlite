import { open, type DB } from '.';

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
    this.db = open({ ...options, name: '__opsqlite_storage' });
    this.db.executeSync('PRAGMA mmap_size=268435456');
    this.db.executeSync(
      'CREATE TABLE IF NOT EXISTS storage (key TEXT PRIMARY KEY, value TEXT) STRICT'
    );
  }

  async getItem(key: string) {
    const result = await this.db.execute(
      'SELECT value FROM storage WHERE key = ?',
      [key]
    );

    return result.rows[0]?.value;
  }

  getItemSync(key: string) {
    const result = this.db.executeSync(
      'SELECT value FROM storage WHERE key = ?',
      [key]
    );

    return result.rows[0]?.value;
  }

  async setItem(key: string, value: string) {
    await this.db.execute(
      'INSERT OR REPLACE INTO storage (key, value) VALUES (?, ?)',
      [key, value]
    );
  }

  setItemSync(key: string, value: string) {
    this.db.executeSync(
      'INSERT OR REPLACE INTO storage (key, value) VALUES (?, ?)',
      [key, value]
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
}
