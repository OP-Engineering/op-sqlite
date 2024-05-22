import Chance from 'chance';
import {open, type DB, type SQLBatchTuple} from '@op-engineering/op-sqlite';
import {beforeEach, describe, it} from './MochaRNAdapter';
import chai from 'chai';

const expect = chai.expect;
const chance = new Chance();
let db: DB;

export function reactiveTests() {
  beforeEach(() => {
    try {
      if (db) {
        db.close();
        db.delete();
      }

      db = open({
        name: 'queries.sqlite',
        encryptionKey: 'test',
      });

      db.execute('DROP TABLE IF EXISTS User;');
      db.execute(
        'CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL, nickname TEXT) STRICT;',
      );
    } catch (e) {
      console.error('error on before each', e);
    }
  });

  describe('Reactive queries', () => {
    it('Basic reactive query', async () => {
      const unsubscribe = db.reactiveExecute({
        query: 'SELECT * FROM User;',
        arguments: [],
        tables: ['User'],
        callback: data => {
          console.log('data', data);
        },
      });
      expect(1).to.equal(0);
    });
  });
}
