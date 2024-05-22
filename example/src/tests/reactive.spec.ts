import {open, type DB} from '@op-engineering/op-sqlite';
import chai from 'chai';
import {beforeEach, describe, it} from './MochaRNAdapter';
import {sleep} from './utils';

const expect = chai.expect;
let db: DB;

export function reactiveTests() {
  beforeEach(() => {
    try {
      if (db) {
        db.close();
        db.delete();
      }

      db = open({
        name: 'reactive.sqlite',
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
    it('Table reactive query', async () => {
      let fullSelectRan = false;
      let emittedUser = null;
      const unsubscribe = db.reactiveExecute({
        query: 'SELECT * FROM User;',
        arguments: [],
        tables: ['User'],
        callback: () => {
          fullSelectRan = true;
        },
      });

      const unsubscribe2 = db.reactiveExecute({
        query: 'SELECT name FROM User;',
        arguments: [],
        tables: ['User'],
        callback: data => {
          emittedUser = data.rows._array[0];
        },
      });

      db.execute(
        'INSERT INTO User (id, name, age, networth, nickname) VALUES (?, ?, ?, ?, ?);',
        [1, 'John', 30, 1000, 'Johnny'],
      );

      await sleep(20);

      db.execute('UPDATE User SET name = ? WHERE id = ?;', ['Foo', 1]);

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
        tables: ['User'],
        callback: () => {
          emittedCount++;
        },
      });

      expect(unsubscribe).to.be.a('function');

      db.execute(
        'INSERT INTO User (id, name, age, networth, nickname) VALUES (?, ?, ?, ?, ?);',
        [1, 'John', 30, 1000, 'Johnny'],
      );

      await sleep(20);

      unsubscribe();

      db.execute('UPDATE User SET name = ? WHERE id = ?;', ['Foo', 1]);

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
        tables: ['User'],
        rowIds: [2],
        callback: () => {
          firstReactiveRan = true;
        },
      });

      const unsubscribe2 = db.reactiveExecute({
        query: 'SELECT * FROM User;',
        arguments: [],
        tables: ['Foo'],
        callback: () => {
          secondReactiveRan = true;
        },
      });

      const unsubscribe3 = db.reactiveExecute({
        query: 'SELECT name FROM User;',
        arguments: [1],
        tables: ['User'],
        rowIds: [1],
        callback: data => {
          emittedUser = data.rows._array[0];
        },
      });

      db.execute(
        'INSERT INTO User (id, name, age, networth, nickname) VALUES (?, ?, ?, ?, ?);',
        [1, 'John', 30, 1000, 'Johnny'],
      );

      await sleep(20);

      db.execute('UPDATE User SET name = ? WHERE id = ?;', ['Foo', 1]);

      await sleep(20);

      expect(firstReactiveRan).to.be.false;
      expect(secondReactiveRan).to.be.false;
      expect(emittedUser).to.deep.eq({
        name: 'Foo',
      });
      unsubscribe();
      unsubscribe2();
      unsubscribe3();
    });
  });
}
