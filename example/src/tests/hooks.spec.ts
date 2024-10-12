import Chance from 'chance';

import {type DB, open, isLibsql} from '@op-engineering/op-sqlite';
import chai from 'chai';
import {describe, it, beforeEach, afterAll, itOnly} from './MochaRNAdapter';
import {sleep} from './utils';

const expect = chai.expect;
const DB_CONFIG = {
  name: 'hooksDb',
  encryptionKey: 'test',
};
const chance = new Chance();

let db: DB;

export function registerHooksTests() {
  describe('Hooks', () => {
    beforeEach(async () => {
      try {
        if (db) {
          db.close();
          db.delete();
        }

        db = open(DB_CONFIG);

        await db.execute('DROP TABLE IF EXISTS User;');
        await db.execute(
          'CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;',
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
    // libsql does not support hooks
    if (isLibsql()) {
      return;
    }

    itOnly('update hook', async () => {
      let promiseResolve: any;
      let promise = new Promise<{
        rowId: number;
        row?: any;
        operation: string;
        table: string;
      }>(resolve => {
        promiseResolve = resolve;
      });
      let db = open({
        name: 'updateHookDb.sqlite',
        encryptionKey: 'blah',
      });

      await db.execute('DROP TABLE IF EXISTS User;');

      await db.execute(
        'CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;',
      );

      db.updateHook(data => {
        promiseResolve(data);
      });

      const id = chance.integer();
      const name = chance.name();
      const age = chance.integer();
      const networth = chance.floating();
      await db.execute(
        'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
        [id, name, age, networth],
      );

      const data = await promise;

      expect(data.operation).to.equal('INSERT');
      expect(data.rowId).to.equal(1);
    });

    it('remove update hook', async () => {
      const hookRes: string[] = [];
      db.updateHook(({operation}) => {
        hookRes.push(operation);
      });

      const id = chance.integer();
      const name = chance.name();
      const age = chance.integer();
      const networth = chance.floating();
      await db.execute(
        'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
        [id, name, age, networth],
      );

      db.updateHook(null);

      await db.execute(
        'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
        [id + 1, name, age, networth],
      );

      await sleep(0);

      expect(hookRes.length).to.equal(1);
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
        await tx.execute(
          'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
          [id, name, age, networth],
        );
      });

      await promise;
    });

    it('remove commit hook', async () => {
      const hookRes: string[] = [];
      db.commitHook(() => {
        hookRes.push('commit');
      });

      const id = chance.integer();
      const name = chance.name();
      const age = chance.integer();
      const networth = chance.floating();
      await db.transaction(async tx => {
        await tx.execute(
          'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
          [id, name, age, networth],
        );
      });

      db.commitHook(null);

      await db.transaction(async tx => {
        await tx.execute(
          'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
          [id + 1, name, age, networth],
        );
      });

      await sleep(0);

      expect(hookRes.length).to.equal(1);
    });

    it('rollback hook', async () => {
      let promiseResolve: any;
      let promise = new Promise(resolve => {
        promiseResolve = resolve;
      });

      db.rollbackHook(() => {
        promiseResolve?.();
      });

      try {
        await db.transaction(async () => {
          throw new Error('Blah');
        });
      } catch (e) {
        // intentionally left blank
      }

      await promise;
    });

    it('remove rollback hook', async () => {
      const hookRes: string[] = [];
      db.rollbackHook(() => {
        hookRes.push('rollback');
      });

      try {
        await db.transaction(async () => {
          throw new Error('Blah');
        });
      } catch (e) {
        // intentionally left blank
      }

      db.rollbackHook(null);

      try {
        await db.transaction(async () => {
          throw new Error('Blah');
        });
      } catch (e) {
        // intentionally left blank
      }

      await sleep(0);

      expect(hookRes.length).to.equal(1);
    });
  });
}
