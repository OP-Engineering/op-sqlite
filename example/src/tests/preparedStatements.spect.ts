import {OPSQLiteConnection, open} from '@op-engineering/op-sqlite';
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
    } catch (e) {
      console.warn('error on before each', e);
    }
  });

  describe('PreparedStatements', () => {
    it('Creates a prepared statement', async () => {
      const statement = db.prepareStatement('SELECT * FROM User;');

      expect(1).to.equal(2);
    });
  });
}
