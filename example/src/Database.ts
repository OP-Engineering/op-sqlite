import performance from 'react-native-performance';
import Chance from 'chance';
import {open} from '@op-engineering/op-sqlcipher';
// import { Buffer } from 'buffer';

const chance = new Chance();

const ROWS = 300000;
const DB_NAME = 'cipherLargeDB';
const ENCRYPTION_KEY = 'quack';
const DB_CONFIG = {
  name: DB_NAME,
  // inMemory: true,
  encryptionKey: ENCRYPTION_KEY,
};

export async function createLargeDB() {
  let largeDb = open(DB_CONFIG);

  largeDb.execute('DROP TABLE IF EXISTS Test;');
  largeDb.execute(
    'CREATE TABLE Test ( id INT PRIMARY KEY, v1 TEXT, v2 TEXT, v3 TEXT, v4 TEXT, v5 TEXT, v6 INT, v7 INT, v8 INT, v9 INT, v10 INT, v11 REAL, v12 REAL, v13 REAL, v14 REAL) STRICT;',
  );

  let insertions: [string, any[]][] = [];
  for (let i = 0; i < ROWS; i++) {
    insertions.push([
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
    ]);
  }

  await largeDb.executeBatchAsync(insertions);

  console.log(`inserted ${ROWS}`);

  largeDb.close();
}

export async function queryLargeDB() {
  let largeDb = open(DB_CONFIG);

  let times: {loadFromDb: number[]; access: number[]} = {
    loadFromDb: [],
    access: [],
  };

  for (let i = 0; i < 1; i++) {
    // @ts-ignore
    global.gc();

    performance.mark('queryStart');
    const results = await largeDb.executeAsync('SELECT * FROM Test');
    const measurement = performance.measure('queryEnd', 'queryStart');
    times.loadFromDb.push(measurement.duration);

    performance.mark('accessingStart');
    const rows = results.rows!._array;
    for (let i = 0; i < rows.length; i++) {
      const v1 = rows[i].v14;
    }

    const accessMeasurement = performance.measure(
      'accessingEnd',
      'accessingStart',
    );
    times.access.push(accessMeasurement.duration);
  }

  return times;
}
