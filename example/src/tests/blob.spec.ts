import {OPSQLiteConnection, open} from '@op-engineering/op-sqlite';
import chai from 'chai';
import {beforeEach, describe, it} from './MochaRNAdapter';

let expect = chai.expect;

let db: OPSQLiteConnection;

export function blobTests() {
  beforeEach(() => {
    try {
      if (db) {
        db.close();
        db.delete();
      }

      db = open({
        name: 'blobs',
      });

      db.execute('DROP TABLE IF EXISTS BlobTable;');
      db.execute(
        'CREATE TABLE BlobTable ( id INT PRIMARY KEY, name TEXT NOT NULL, content BLOB) STRICT;',
      );
    } catch (e) {
      console.warn('error on before each', e);
    }
  });

  describe('Blobs', () => {
    it('Should be able to insert blobs', async () => {
      let buffer = new ArrayBuffer(24);
      let content = new Uint8Array(buffer, 4, 16);
      // @ts-ignore
      crypto.getRandomValues(content);

      db.execute(`INSERT OR REPLACE INTO BlobTable VALUES (?, ?, ?);`, [
        1,
        'myTestBlob',
        buffer,
      ]);

      expect(1).to.equal(1);
    });
  });
}
