import Chance from 'chance';

import {OPSQLiteConnection, open} from '@op-engineering/op-sqlite';
import chai from 'chai';
import {describe, it, beforeEach} from './MochaRNAdapter';

async function sleep(ms: number): Promise<void> {
  return new Promise<void>(resolve => {
    setTimeout(resolve, ms);
  });
}

const expect = chai.expect;
const DB_CONFIG = {
  name: 'updateHookDb',
};
const chance = new Chance();

let db: OPSQLiteConnection;

export function updateHookTests() {
  beforeEach(() => {
    try {
      if (db) {
        db.close();
        db.delete();
      }

      db = open(DB_CONFIG);

      db.execute('DROP TABLE IF EXISTS User;');
      db.execute(
        'CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;',
      );
    } catch (e) {
      console.warn('error on before each', e);
    }
  });

  describe('Update hook', () => {
    it('Should register an update hook', async () => {
      let promiseResolve: any;
      let promise = new Promise(resolve => {
        promiseResolve = resolve;
      });
      db.updateHook(({rowId, table, operation, row = {}}) => {
        // console.warn(
        //   `Hook has been called, rowId: ${rowId}, ${table}, ${operation}`,
        // );
        // console.warn(JSON.stringify(row, null, 2));
        promiseResolve?.(operation);
      });

      const id = chance.integer();
      const name = chance.name();
      const age = chance.integer();
      const networth = chance.floating();
      db.execute(
        'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
        [id, name, age, networth],
      );

      const operation = await promise;

      expect(operation).to.equal('INSERT');
    });
  });
}
