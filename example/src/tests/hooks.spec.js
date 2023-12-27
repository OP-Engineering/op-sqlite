"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.registerHooksTests = void 0;
const chance_1 = __importDefault(require("chance"));
const op_sqlcipher_1 = require("@op-engineering/op-sqlcipher");
const chai_1 = __importDefault(require("chai"));
const MochaRNAdapter_1 = require("./MochaRNAdapter");
async function sleep(ms) {
    return new Promise(resolve => {
        setTimeout(resolve, ms);
    });
}
const expect = chai_1.default.expect;
const DB_CONFIG = {
    name: 'hooksDb',
};
const chance = new chance_1.default();
let db;
function registerHooksTests() {
    (0, MochaRNAdapter_1.beforeEach)(() => {
        try {
            if (db) {
                db.close();
                db.delete();
            }
            db = (0, op_sqlcipher_1.open)(DB_CONFIG);
            db.execute('DROP TABLE IF EXISTS User;');
            db.execute('CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;');
        }
        catch (e) {
            console.warn('error on before each', e);
        }
    });
    (0, MochaRNAdapter_1.describe)('Hooks', () => {
        (0, MochaRNAdapter_1.it)('update hook', async () => {
            let promiseResolve;
            let promise = new Promise(resolve => {
                promiseResolve = resolve;
            });
            db.updateHook(({ rowId, table, operation, row = {} }) => {
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
            db.execute('INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)', [id, name, age, networth]);
            const operation = await promise;
            expect(operation).to.equal('INSERT');
        });
        (0, MochaRNAdapter_1.it)('remove update hook', async () => {
            const hookRes = [];
            db.updateHook(({ rowId, table, operation, row = {} }) => {
                hookRes.push(operation);
            });
            const id = chance.integer();
            const name = chance.name();
            const age = chance.integer();
            const networth = chance.floating();
            db.execute('INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)', [id, name, age, networth]);
            db.updateHook(null);
            db.execute('INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)', [id + 1, name, age, networth]);
            await sleep(0);
            expect(hookRes.length).to.equal(1);
        });
        (0, MochaRNAdapter_1.it)('commit hook', async () => {
            let promiseResolve;
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
            await db.transaction(async (tx) => {
                tx.execute('INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)', [id, name, age, networth]);
            });
            await promise;
        });
        (0, MochaRNAdapter_1.it)('remove commit hook', async () => {
            const hookRes = [];
            db.commitHook(() => {
                hookRes.push('commit');
            });
            const id = chance.integer();
            const name = chance.name();
            const age = chance.integer();
            const networth = chance.floating();
            await db.transaction(async (tx) => {
                tx.execute('INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)', [id, name, age, networth]);
            });
            db.commitHook(null);
            await db.transaction(async (tx) => {
                tx.execute('INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)', [id + 1, name, age, networth]);
            });
            await sleep(0);
            expect(hookRes.length).to.equal(1);
        });
        (0, MochaRNAdapter_1.it)('rollback hook', async () => {
            let promiseResolve;
            let promise = new Promise(resolve => {
                promiseResolve = resolve;
            });
            db.rollbackHook(() => {
                promiseResolve?.();
            });
            try {
                await db.transaction(async (tx) => {
                    throw new Error('Blah');
                });
            }
            catch (e) {
                // intentionally left blank
            }
            await promise;
        });
        (0, MochaRNAdapter_1.it)('remove rollback hook', async () => {
            const hookRes = [];
            db.rollbackHook(() => {
                hookRes.push('rollback');
            });
            try {
                await db.transaction(async (tx) => {
                    throw new Error('Blah');
                });
            }
            catch (e) {
                // intentionally left blank
            }
            db.rollbackHook(null);
            try {
                await db.transaction(async (tx) => {
                    throw new Error('Blah');
                });
            }
            catch (e) {
                // intentionally left blank
            }
            await sleep(0);
            expect(hookRes.length).to.equal(1);
        });
    });
}
exports.registerHooksTests = registerHooksTests;
