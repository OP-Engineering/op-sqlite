"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.blobTests = void 0;
const op_sqlcipher_1 = require("@op-engineering/op-sqlcipher");
const chai_1 = __importDefault(require("chai"));
const MochaRNAdapter_1 = require("./MochaRNAdapter");
let expect = chai_1.default.expect;
let db;
function blobTests() {
    (0, MochaRNAdapter_1.beforeEach)(() => {
        try {
            if (db) {
                db.close();
                db.delete();
            }
            db = (0, op_sqlcipher_1.open)({
                name: 'cipherBlobs',
                encryptionKey: 'quack',
            });
            db.execute('DROP TABLE IF EXISTS BlobTable;');
            db.execute('CREATE TABLE BlobTable ( id INT PRIMARY KEY, content BLOB) STRICT;');
        }
        catch (e) {
            console.warn('error on before each', e);
        }
    });
    (0, MochaRNAdapter_1.describe)('Blobs', () => {
        (0, MochaRNAdapter_1.it)('ArrayBuffer', async () => {
            const uint8 = new Uint8Array(2);
            uint8[0] = 42;
            db.execute(`INSERT OR REPLACE INTO BlobTable VALUES (?, ?);`, [
                1,
                uint8.buffer,
            ]);
            const result = db.execute('SELECT content FROM BlobTable');
            const finalUint8 = new Uint8Array(result.rows._array[0].content);
            expect(finalUint8[0]).to.equal(42);
        });
        (0, MochaRNAdapter_1.it)('Uint8Array', async () => {
            const uint8 = new Uint8Array(2);
            uint8[0] = 42;
            db.execute(`INSERT OR REPLACE INTO BlobTable VALUES (?, ?);`, [1, uint8]);
            const result = db.execute('SELECT content FROM BlobTable');
            const finalUint8 = new Uint8Array(result.rows._array[0].content);
            expect(finalUint8[0]).to.equal(42);
        });
        (0, MochaRNAdapter_1.it)('Uint16Array', async () => {
            const uint8 = new Uint16Array(2);
            uint8[0] = 42;
            db.execute(`INSERT OR REPLACE INTO BlobTable VALUES (?, ?);`, [1, uint8]);
            const result = db.execute('SELECT content FROM BlobTable');
            const finalUint8 = new Uint8Array(result.rows._array[0].content);
            expect(finalUint8[0]).to.equal(42);
        });
    });
}
exports.blobTests = blobTests;
