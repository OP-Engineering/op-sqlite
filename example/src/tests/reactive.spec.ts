import {isLibsql, open, type DB} from '@op-engineering/op-sqlite';
import chai from 'chai';
import {afterAll, beforeEach, describe, it} from './MochaRNAdapter';
import {sleep} from './utils';
import Chance from 'chance';

const expect = chai.expect;
const chance = new Chance();
let db: DB;

export function reactiveTests() {
  describe('Reactive queries', () => {
    beforeEach(async () => {
      if (db) {
        db.close();
        db.delete();
      }

      db = open({
        name: 'reactive.sqlite',
        encryptionKey: 'test',
      });

      await db.execute('DROP TABLE IF EXISTS User;');
      await db.execute(
        'CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL, nickname TEXT) STRICT;',
      );
    });

    afterAll(() => {
      if (db) {
        db.close();
        db.delete();
        // @ts-ignore
        db = null;
      }
    });
    // libsql does not support reactive queries
    if (isLibsql()) {
      return;
    }

    it('Table reactive query', async () => {
      let fullSelectRan = false;
      let emittedUser = null;
      const unsubscribe = db.reactiveExecute({
        query: 'SELECT * FROM User;',
        arguments: [],
        fireOn: [
          {
            table: 'User',
          },
        ],
        callback: data => {
          fullSelectRan = true;
        },
      });

      const unsubscribe2 = db.reactiveExecute({
        query: 'SELECT name FROM User;',
        arguments: [],
        fireOn: [
          {
            table: 'User',
          },
        ],
        callback: data => {
          emittedUser = data.rows[0];
        },
      });

      await db.transaction(async tx => {
        await tx.execute(
          'INSERT INTO User (id, name, age, networth, nickname) VALUES (?, ?, ?, ?, ?);',
          [1, 'John', 30, 1000, 'Johnny'],
        );
      });

      await sleep(20);

      await db.transaction(async tx => {
        await tx.execute('UPDATE User SET name = ? WHERE id = ?;', ['Foo', 1]);
      });

      await sleep(20);

      expect(fullSelectRan).to.be.true;
      expect(emittedUser).to.deep.eq({
        name: 'Foo',
      });

      unsubscribe();
      unsubscribe2();
    });

    it('Can unsubscribe from reactive query', async () => {
      let emittedCount = 0;
      const unsubscribe = db.reactiveExecute({
        query: 'SELECT * FROM User;',
        arguments: [],
        fireOn: [
          {
            table: 'User',
          },
        ],
        callback: () => {
          emittedCount++;
        },
      });

      expect(unsubscribe).to.be.a('function');

      await db.transaction(async tx => {
        await tx.execute(
          'INSERT INTO User (id, name, age, networth, nickname) VALUES (?, ?, ?, ?, ?);',
          [1, 'John', 30, 1000, 'Johnny'],
        );
      });

      await sleep(20);

      unsubscribe();

      await db.transaction(async tx => {
        await tx.execute('UPDATE User SET name = ? WHERE id = ?;', ['Foo', 1]);
      });
      await sleep(20);
      expect(emittedCount).to.eq(1);
    });

    it('Row reactive query', async () => {
      let firstReactiveRan = false;
      let secondReactiveRan = false;
      let emittedUser = null;

      const unsubscribe = db.reactiveExecute({
        query: 'SELECT * FROM User;',
        arguments: [],
        fireOn: [
          {
            table: 'User',
            ids: [2],
          },
        ],
        callback: () => {
          firstReactiveRan = true;
        },
      });

      const unsubscribe2 = db.reactiveExecute({
        query: 'SELECT * FROM User;',
        arguments: [],
        fireOn: [
          {
            table: 'Foo',
          },
        ],
        callback: () => {
          secondReactiveRan = true;
        },
      });

      const unsubscribe3 = db.reactiveExecute({
        query: 'SELECT name FROM User;',
        arguments: [],
        fireOn: [
          {
            table: 'User',
            ids: [1],
          },
        ],
        callback: data => {
          emittedUser = data.rows[0];
        },
      });

      await db.transaction(async tx => {
        await tx.execute(
          'INSERT INTO User (id, name, age, networth, nickname) VALUES (?, ?, ?, ?, ?);',
          [1, 'John', 30, 1000, 'Johnny'],
        );
      });

      await sleep(0);

      await db.transaction(async tx => {
        await tx.execute('UPDATE User SET name = ? WHERE id = ?;', ['Foo', 1]);
      });

      await sleep(0);

      expect(firstReactiveRan).to.be.false;
      expect(secondReactiveRan).to.be.false;
      expect(emittedUser).to.deep.eq({
        name: 'Foo',
      });
      unsubscribe();
      unsubscribe2();
      unsubscribe3();
    });

    it('Update hook and reactive queries work at the same time', async () => {
      let promiseResolve: any;
      let promise = new Promise(resolve => {
        promiseResolve = resolve;
      });
      db.updateHook(({operation}) => {
        promiseResolve?.(operation);
      });

      let emittedUser = null;
      const unsubscribe = db.reactiveExecute({
        query: 'SELECT * FROM User;',
        arguments: [],
        fireOn: [
          {
            table: 'User',
          },
        ],
        callback: data => {
          emittedUser = data.rows[0];
        },
      });

      const id = chance.integer({
        min: 1,
        max: 100000,
      });
      const name = chance.name();
      const age = chance.integer();
      const networth = chance.floating();

      await db.transaction(async tx => {
        await tx.execute(
          'INSERT INTO User (id, name, age, networth, nickname) VALUES (?, ?, ?, ?, ?);',
          [id, name, age, networth, 'Johnny'],
        );
      });

      const operation = await promise;

      await sleep(0);

      expect(operation).to.equal('INSERT');
      expect(emittedUser).to.deep.eq({
        id,
        name,
        age,
        networth,
        nickname: 'Johnny',
      });

      unsubscribe();
    });
  });
}
