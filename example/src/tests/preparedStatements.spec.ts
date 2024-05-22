import {type DB, open} from '@op-engineering/op-sqlite';
import chai from 'chai';
import {beforeEach, describe, it} from './MochaRNAdapter';

let expect = chai.expect;

let db: DB;

export function preparedStatementsTests() {
  beforeEach(() => {
    try {
      if (db) {
        db.close();
        db.delete();
      }

      db = open({
        name: 'statements',
        encryptionKey: 'test',
      });

      db.execute('DROP TABLE IF EXISTS User;');
      db.execute('CREATE TABLE User ( id INT PRIMARY KEY, name TEXT) STRICT;');
      db.execute('INSERT INTO "User" (id, name) VALUES(?,?)', [1, 'Oscar']);
      db.execute('INSERT INTO "User" (id, name) VALUES(?,?)', [2, 'Pablo']);
      db.execute('INSERT INTO "User" (id, name) VALUES(?,?)', [3, 'Carlos']);
    } catch (e) {
      console.warn('error on before each', e);
    }
  });

  describe('PreparedStatements', () => {
    it('Creates a prepared statement and executes a prepared statement', async () => {
      const statement = db.prepareStatement('SELECT * FROM User;');
      let results = statement.execute();

      expect(results.rows!._array.length).to.equal(3);
      results = statement.execute();

      expect(results.rows!._array.length).to.equal(3);
    });

    it('prepared statement, rebind select', async () => {
      const statement = db.prepareStatement('SELECT * FROM User WHERE id=?;');
      statement.bind([1]);

      let results = statement.execute();
      expect(results.rows!._array[0].name === 'Oscar');

      statement.bind([2]);
      results = statement.execute();
      expect(results.rows!._array[0].name === 'Pablo');
    });

    it('prepared statement, rebind insert', async () => {
      const statement = db.prepareStatement(
        'INSERT INTO "User" (id, name) VALUES(?,?);',
      );
      statement.bind([4, 'Juan']);
      statement.execute();

      statement.bind([5, 'Pedro']);
      statement.execute();
    });
  });
}
