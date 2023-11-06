import performance from 'react-native-performance';
import Chance from 'chance';
import {open} from 'op-sqlite';
// import { Buffer } from 'buffer';

const chance = new Chance();

export async function createLargeDB() {
  let largeDb = open({
    name: 'largeDB',
  });

  largeDb.execute('DROP TABLE IF EXISTS Test;');
  largeDb.execute(
    'CREATE TABLE Test ( id INT PRIMARY KEY, v1 TEXT, v2 TEXT, v3 TEXT, v4 TEXT, v5 TEXT, v6 INT, v7 INT, v8 INT, v9 INT, v10 INT, v11 REAL, v12 REAL, v13 REAL, v14 REAL) STRICT;',
  );

  for (let i = 0; i < 300000; i++) {
    await largeDb.executeAsync(
      'INSERT INTO "Test" (id, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)',
      [
        i,
        chance.name(),
        chance.name(),
        chance.name(),
        chance.name(),
        chance.name(),
        chance.integer(),
        chance.integer(),
        chance.integer(),
        chance.integer(),
        chance.integer(),
        chance.floating(),
        chance.floating(),
        chance.floating(),
        chance.floating(),
      ],
    );
  }

  largeDb.close();
}

export async function queryLargeDB() {
  let largeDb = open({
    name: 'largeDB',
  });

  let times = [];
  for (let i = 0; i < 10; i++) {
    performance.mark('queryStart');
    await largeDb.executeAsync('SELECT * FROM Test');
    performance.measure('queryEnd', 'queryStart');
    let entries = performance.getEntriesByName('queryEnd');

    times.push(entries[entries.length - 1].duration);
    // @ts-ignore
    global.gc();
  }

  return times;
}
