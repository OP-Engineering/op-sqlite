import {OPSQLiteConnection, open} from '@op-engineering/op-sqlite';
import chai from 'chai';
import {beforeEach, describe, it} from './MochaRNAdapter';

let expect = chai.expect;

let db: OPSQLiteConnection;

function areBuffersEqual(buf1: ArrayBuffer, buf2: ArrayBuffer) {
  if (buf1.byteLength != buf2.byteLength) return false;
  var dv1 = new Uint8Array(buf1);
  var dv2 = new Uint8Array(buf2);
  for (var i = 0; i != buf1.byteLength; i++) {
    if (dv1[i] != dv2[i]) return false;
  }
  return true;
}

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
        'CREATE TABLE BlobTable ( id INT PRIMARY KEY, content BLOB) STRICT;',
      );
    } catch (e) {
      console.warn('error on before each', e);
    }
  });

  describe('Blobs', () => {
    it('ArrayBuffer', async () => {
      const uint8 = new Uint8Array(2);
      uint8[0] = 42;

      db.execute(`INSERT OR REPLACE INTO BlobTable VALUES (?, ?);`, [
        1,
        uint8.buffer,
      ]);

      const result = db.execute('SELECT content FROM BlobTable');

      const finalUint8 = new Uint8Array(result.rows!._array[0].content);
      expect(finalUint8[0]).to.equal(42);
    });

    it('Uint8Array', async () => {
      const uint8 = new Uint8Array(2);
      uint8[0] = 42;

      db.execute(`INSERT OR REPLACE INTO BlobTable VALUES (?, ?);`, [1, uint8]);

      const result = db.execute('SELECT content FROM BlobTable');

      const finalUint8 = new Uint8Array(result.rows!._array[0].content);
      expect(finalUint8[0]).to.equal(42);
    });

    it('Uint16Array', async () => {
      const uint8 = new Uint16Array(2);
      uint8[0] = 42;

      db.execute(`INSERT OR REPLACE INTO BlobTable VALUES (?, ?);`, [1, uint8]);

      const result = db.execute('SELECT content FROM BlobTable');

      const finalUint8 = new Uint8Array(result.rows!._array[0].content);
      expect(finalUint8[0]).to.equal(42);
    });
  });
}
