"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.queryLargeDB = exports.createLargeDB = void 0;
const react_native_performance_1 = __importDefault(require("react-native-performance"));
const chance_1 = __importDefault(require("chance"));
const op_sqlcipher_1 = require("@op-engineering/op-sqlcipher");
// import { Buffer } from 'buffer';
const chance = new chance_1.default();
const ROWS = 300000;
const DB_NAME = 'cipherLargeDB';
const ENCRYPTION_KEY = 'quack';
const DB_CONFIG = {
    name: DB_NAME,
    encryptionKey: ENCRYPTION_KEY,
};
async function createLargeDB() {
    let largeDb = (0, op_sqlcipher_1.open)(DB_CONFIG);
    largeDb.execute('DROP TABLE IF EXISTS Test;');
    largeDb.execute('CREATE TABLE Test ( id INT PRIMARY KEY, v1 TEXT, v2 TEXT, v3 TEXT, v4 TEXT, v5 TEXT, v6 INT, v7 INT, v8 INT, v9 INT, v10 INT, v11 REAL, v12 REAL, v13 REAL, v14 REAL) STRICT;');
    largeDb.execute('PRAGMA mmap_size=268435456');
    let insertions = [];
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
    largeDb.close();
}
exports.createLargeDB = createLargeDB;
async function queryLargeDB() {
    let largeDb = (0, op_sqlcipher_1.open)(DB_CONFIG);
    largeDb.execute('PRAGMA mmap_size=268435456');
    let times = {
        loadFromDb: [],
        access: [],
        prepare: [],
        preparedExecution: [],
    };
    for (let i = 0; i < 10; i++) {
        // @ts-ignore
        global.gc();
        react_native_performance_1.default.mark('queryStart');
        const results = await largeDb.executeAsync('SELECT * FROM Test');
        const measurement = react_native_performance_1.default.measure('queryEnd', 'queryStart');
        times.loadFromDb.push(measurement.duration);
        // @ts-ignore
        global.gc();
        react_native_performance_1.default.mark('accessingStart');
        const rows = results.rows._array;
        for (let i = 0; i < rows.length; i++) {
            const v1 = rows[i].v14;
        }
        const accessMeasurement = react_native_performance_1.default.measure('accessingEnd', 'accessingStart');
        times.access.push(accessMeasurement.duration);
        // @ts-ignore
        global.gc();
        let start = react_native_performance_1.default.now();
        const statement = largeDb.prepareStatement('SELECT * FROM Test');
        let end = react_native_performance_1.default.now();
        times.prepare.push(end - start);
        // @ts-ignore
        global.gc();
        start = react_native_performance_1.default.now();
        let results2 = statement.execute();
        end = react_native_performance_1.default.now();
        times.preparedExecution.push(end - start);
    }
    return times;
}
exports.queryLargeDB = queryLargeDB;
