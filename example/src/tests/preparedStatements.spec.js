"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.preparedStatementsTests = void 0;
const op_sqlcipher_1 = require("@op-engineering/op-sqlcipher");
const chai_1 = __importDefault(require("chai"));
const MochaRNAdapter_1 = require("./MochaRNAdapter");
let expect = chai_1.default.expect;
let db;
function preparedStatementsTests() {
    (0, MochaRNAdapter_1.beforeEach)(() => {
        try {
            if (db) {
                db.close();
                db.delete();
            }
            db = (0, op_sqlcipher_1.open)({
                name: 'statements',
            });
            db.execute('DROP TABLE IF EXISTS User;');
            db.execute('CREATE TABLE User ( id INT PRIMARY KEY, name TEXT) STRICT;');
            db.execute('INSERT INTO "User" (id, name) VALUES(?,?)', [1, 'Oscar']);
            db.execute('INSERT INTO "User" (id, name) VALUES(?,?)', [2, 'Pablo']);
            db.execute('INSERT INTO "User" (id, name) VALUES(?,?)', [3, 'Carlos']);
        }
        catch (e) {
            console.warn('error on before each', e);
        }
    });
    (0, MochaRNAdapter_1.describe)('PreparedStatements', () => {
        (0, MochaRNAdapter_1.it)('Creates a prepared statement and executes a prepared statement', async () => {
            const statement = db.prepareStatement('SELECT * FROM User;');
            let results = statement.execute();
            expect(results.rows._array.length).to.equal(3);
            results = statement.execute();
            expect(results.rows._array.length).to.equal(3);
        });
    });
}
exports.preparedStatementsTests = preparedStatementsTests;
