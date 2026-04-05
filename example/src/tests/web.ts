import {open, openAsync, type DB} from '@op-engineering/op-sqlite';
import {describe, expect, it} from '@op-engineering/op-test';
import {Platform} from 'react-native';

describe('Web backend', () => {
  if (Platform.OS !== 'web') {
    return;
  }

  it('opens with openAsync and executes queries', async () => {
    const db = await openAsync({
      name: 'web-tests.sqlite',
    });

    await db.execute(
      'CREATE TABLE IF NOT EXISTS WebTests (id INTEGER PRIMARY KEY, value TEXT NOT NULL)',
    );
    await db.execute('DELETE FROM WebTests');
    await db.execute('INSERT INTO WebTests (id, value) VALUES (?, ?)', [1, 'ok']);

    const result = await db.execute('SELECT value FROM WebTests WHERE id = ?', [1]);

    expect(result.rows[0]?.value).toEqual('ok');

    await db.closeAsync();
  });

  it('open() throws on web', () => {
    let didThrow = false;

    try {
      open({
        name: 'web-tests.sqlite',
      });
    } catch (error) {
      didThrow = true;
      expect((error as Error).message.includes('async-only')).toEqual(true);
    }

    expect(didThrow).toEqual(true);
  });

  it('executeSync() throws on web', async () => {
    let db: DB | null = null;

    try {
      db = await openAsync({
        name: 'web-tests.sqlite',
      });

      let didThrow = false;

      try {
        db.executeSync('SELECT 1');
      } catch (_error) {
        didThrow = true;
      }

      expect(didThrow).toEqual(true);
    } finally {
      await db?.closeAsync();
    }
  });

  it('rejects SQLCipher options on web', async () => {
    let didThrow = false;

    try {
      await openAsync({
        name: 'web-tests-encrypted.sqlite',
        encryptionKey: 'not-supported-on-web',
      });
    } catch (error) {
      didThrow = true;
      expect((error as Error).message.includes('SQLCipher')).toEqual(true);
    }

    expect(didThrow).toEqual(true);
  });
});
