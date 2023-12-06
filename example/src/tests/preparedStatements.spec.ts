import {OPSQLiteConnection, open} from '@op-engineering/op-sqlcipher';
import chai from 'chai';
import {beforeEach, describe, it} from './MochaRNAdapter';

let expect = chai.expect;

let db: OPSQLiteConnection;

export function preparedStatementsTests() {
  beforeEach(() => {
    try {
      if (db) {
        db.close();
        db.delete();
      }

      db = open({
        name: 'statements',
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
  });
}
