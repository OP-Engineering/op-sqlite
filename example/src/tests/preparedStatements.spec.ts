import {open, type DB} from '@op-engineering/op-sqlite';
import chai from 'chai';
import {afterAll, beforeEach, describe, it} from './MochaRNAdapter';

let expect = chai.expect;

let db: DB;

export function preparedStatementsTests() {
  beforeEach(async () => {
    try {
      if (db) {
        db.close();
        db.delete();
      }

      db = open({
        name: 'statements',
        encryptionKey: 'test',
      });

      await db.execute('DROP TABLE IF EXISTS User;');
      await db.execute(
        'CREATE TABLE User ( id INT PRIMARY KEY, name TEXT) STRICT;',
      );
      await db.execute('INSERT INTO "User" (id, name) VALUES(?,?)', [
        1,
        'Oscar',
      ]);
      await db.execute('INSERT INTO "User" (id, name) VALUES(?,?)', [
        2,
        'Pablo',
      ]);
      await db.execute('INSERT INTO "User" (id, name) VALUES(?,?)', [
        3,
        'Carlos',
      ]);
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

  describe('PreparedStatements', () => {
    it('Creates a prepared statement and executes a prepared statement', async () => {
      const statement = db.prepareStatement('SELECT * FROM User;');
      let results = await statement.execute();

      expect(results.rows!._array.length).to.equal(3);
      results = await statement.execute();

      expect(results.rows!._array.length).to.equal(3);
    });

    it('prepared statement, rebind select', async () => {
      const statement = db.prepareStatement('SELECT * FROM User WHERE id=?;');
      statement.bind([1]);

      let results = await statement.execute();
      expect(results.rows!._array[0].name === 'Oscar');

      statement.bind([2]);
      results = await statement.execute();
      expect(results.rows!._array[0].name === 'Pablo');
    });

    it('prepared statement, rebind insert', async () => {
      const statement = db.prepareStatement(
        'INSERT INTO "User" (id, name) VALUES(?,?);',
      );
      statement.bind([4, 'Juan']);
      await statement.execute();

      statement.bind([5, 'Pedro']);
      await statement.execute();
    });
  });
}
