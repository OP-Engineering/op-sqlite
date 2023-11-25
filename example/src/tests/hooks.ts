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
  name: 'hooksDb',
};
const chance = new Chance();

let db: OPSQLiteConnection;

export function registerHooksTests() {
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

  describe('Hooks', () => {
    it('update hook', async () => {
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

    it('remove update hook', async () => {
      const hookRes: string[] = []
      db.updateHook(({ rowId, table, operation, row = {} }) => {
        hookRes.push(operation);
      });

      const id = chance.integer();
      const name = chance.name();
      const age = chance.integer();
      const networth = chance.floating();
      db.execute(
        'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
        [id, name, age, networth],
      );

      db.updateHook(null)

      db.execute(
        'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
        [id + 1, name, age, networth],
      );

      await sleep(0)

      expect(hookRes.length).to.equal(1)
    });

    it('commit hook', async () => {
      let promiseResolve: any;
      let promise = new Promise(resolve => {
        promiseResolve = resolve;
      });

      db.commitHook(() => {
        promiseResolve?.();
      });

      const id = chance.integer();
      const name = chance.name();
      const age = chance.integer();
      const networth = chance.floating();
      await db.transaction(async tx => {
        tx.execute(
          'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
          [id, name, age, networth],
        );
      });

      await promise;
    });

    it('remove commit hook', async () => {
      const hookRes: string[] = []
      db.commitHook(() => {
        hookRes.push('commit');
      });

      const id = chance.integer();
      const name = chance.name();
      const age = chance.integer();
      const networth = chance.floating();
      await db.transaction(async tx => {
        tx.execute(
          'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
          [id, name, age, networth],
        );
      });

      db.commitHook(null)

      await db.transaction(async tx => {
        tx.execute(
          'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
          [id+1, name, age, networth],
        );
      });

      await sleep(0)

      expect(hookRes.length).to.equal(1)
    });

    it('rollback hook', async () => {
      let promiseResolve: any;
      let promise = new Promise(resolve => {
        promiseResolve = resolve;
      });

      db.rollbackHook(() => {
        promiseResolve?.();
      });

      const id = chance.integer();
      const name = chance.name();
      const age = chance.integer();
      const networth = chance.floating();
      console.warn('1');
      try {
        await db.transaction(async tx => {
          throw new Error('Blah');
        });
      } catch (e) {
        // intentionally left blank
      }

      console.warn('2');
      await promise;
    });

    it('remove rollback hook', async () => {
      const hookRes: string[] = []
      db.rollbackHook(() => {
        hookRes.push('rollback');
      });

      try {
        await db.transaction(async tx => {
          throw new Error('Blah');
        });
      } catch (e) {
        // intentionally left blank
      }

      db.rollbackHook(null)

      try {
        await db.transaction(async tx => {
          throw new Error('Blah');
        });
      } catch (e) {
        // intentionally left blank
      }

      await sleep(0)

      expect(hookRes.length).to.equal(1)
    });
  });
}
