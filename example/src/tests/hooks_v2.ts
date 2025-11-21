import Chance from 'chance';

import {
  type DB,
  openV2,
  isLibsql,
  IOS_DOCUMENT_PATH,
  ANDROID_DATABASE_PATH,
} from '@op-engineering/op-sqlite';
import {
  expect,
  describe,
  it,
  beforeEach,
  afterEach,
} from '@op-engineering/op-test';
import {sleep} from './utils';
import {Platform} from 'react-native';

const DB_CONFIG = {
  path:
    Platform.OS === 'ios'
      ? `${IOS_DOCUMENT_PATH}/hooks2.sqlite`
      : `${ANDROID_DATABASE_PATH}/hooks2.sqlite`,
  encryptionKey: 'test',
};
const chance = new Chance();

describe('Hooks V2', () => {
  let db: DB;
  if (isLibsql()) {
    return;
  }

  beforeEach(async () => {
    try {
      db = openV2(DB_CONFIG);

      await db.execute('DROP TABLE IF EXISTS User;');
      await db.execute(
        'CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;',
      );
    } catch (e) {
      console.warn('error on before each', e);
    }
  });

  afterEach(() => {
    if (db) {
      db.delete();
    }
  });

  it('update hook', async () => {
    let promiseResolve: any;
    let promise = new Promise<{
      rowId: number;
      row?: any;
      operation: string;
      table: string;
    }>(resolve => {
      promiseResolve = resolve;
    });

    db.updateHook(data => {
      promiseResolve(data);
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

    const data = await promise;

    expect(data.operation).toEqual('INSERT');
    expect(data.rowId).toEqual(1);

    db.updateHook(null);
  });

  it('Execute batch should trigger update hook', async () => {
    const id = chance.integer();
    const name = chance.name();
    const age = chance.integer();
    const networth = chance.floating();

    db.executeSync(
      'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
      [id, name, age, networth],
    );

    let promiseResolve: any;
    let promise = new Promise<{
      rowId: number;
      row?: any;
      operation: string;
      table: string;
    }>(resolve => {
      promiseResolve = resolve;
    });

    db.updateHook(data => {
      promiseResolve(data);
    });

    await db.executeBatch([
      ['UPDATE "User" SET name = ? WHERE id = ?', ['foo', id]],
    ]);

    const data = await promise;

    expect(data.operation).toEqual('UPDATE');
    expect(data.rowId).toEqual(1);
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
    await db.transaction(async tx => {
      await tx.execute(
        'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
        [id, name, age, networth],
      );
    });

    db.updateHook(null);

    await db.transaction(async tx => {
      await tx.execute(
        'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
        [id + 1, name, age, networth],
      );
    });

    await sleep(0);

    expect(hookRes.length).toEqual(1);
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
    db.commitHook(null);
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

    expect(hookRes.length).toEqual(1);
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

    expect(hookRes.length).toEqual(1);
  });
});
