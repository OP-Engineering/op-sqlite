import {type DB, open} from '@op-engineering/op-sqlite';
import chai from 'chai';
import {afterAll, beforeEach, describe, it} from './MochaRNAdapter';

let expect = chai.expect;

let db: DB;

export function blobTests() {
  beforeEach(async () => {
    try {
      if (db) {
        db.close();
        db.delete();
      }

      db = open({
        name: 'blobs',
        encryptionKey: 'test',
      });

      await db.execute('DROP TABLE IF EXISTS BlobTable;');
      await db.execute(
        'CREATE TABLE BlobTable ( id INT PRIMARY KEY, content BLOB) STRICT;',
      );
    } catch (e) {
      console.warn('error on before each', e);
    }
  });

  afterAll(() => {
    if (db) {
      db.close();
      db.delete();
      // @ts-ignore
      db = null;
    }
  });

  describe('Blobs', () => {
    it('ArrayBuffer', async () => {
      const uint8 = new Uint8Array(2);
      uint8[0] = 42;

      await db.execute(`INSERT OR REPLACE INTO BlobTable VALUES (?, ?);`, [
        1,
        uint8.buffer,
      ]);

      const result = await db.execute('SELECT content FROM BlobTable');

      const finalUint8 = new Uint8Array(result.rows!._array[0].content);
      expect(finalUint8[0]).to.equal(42);
    });

    it('Uint8Array', async () => {
      const uint8 = new Uint8Array(2);
      uint8[0] = 42;

      await db.execute(`INSERT OR REPLACE INTO BlobTable VALUES (?, ?);`, [
        1,
        uint8,
      ]);

      const result = await db.execute('SELECT content FROM BlobTable');

      const finalUint8 = new Uint8Array(result.rows!._array[0].content);
      expect(finalUint8[0]).to.equal(42);
    });

    it('Uint16Array', async () => {
      const uint8 = new Uint16Array(2);
      uint8[0] = 42;

      await db.execute(`INSERT OR REPLACE INTO BlobTable VALUES (?, ?);`, [
        1,
        uint8,
      ]);

      const result = await db.execute('SELECT content FROM BlobTable');

      const finalUint8 = new Uint8Array(result.rows!._array[0].content);
      expect(finalUint8[0]).to.equal(42);
    });

    it('Uint8Array in prepared statement', async () => {
      const uint8 = new Uint8Array(2);
      uint8[0] = 46;

      const statement = db.prepareStatement(
        'INSERT OR REPLACE INTO BlobTable VALUES (?, ?);',
      );
      statement.bind([1, uint8]);

      await statement.execute();

      const result = await db.execute('SELECT content FROM BlobTable');

      const finalUint8 = new Uint8Array(result.rows!._array[0].content);
      expect(finalUint8[0]).to.equal(46);
    });

    it('Buffer in prepared statement', async () => {
      const uint8 = new Uint8Array(2);
      uint8[0] = 52;

      const statement = db.prepareStatement(
        'INSERT OR REPLACE INTO BlobTable VALUES (?, ?);',
      );
      statement.bind([1, uint8.buffer]);

      await statement.execute();

      const result = await db.execute('SELECT content FROM BlobTable');

      const finalUint8 = new Uint8Array(result.rows!._array[0].content);
      expect(finalUint8[0]).to.equal(52);
    });
  });
}
