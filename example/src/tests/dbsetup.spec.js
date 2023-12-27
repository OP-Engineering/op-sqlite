"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.dbSetupTests = void 0;
const op_sqlcipher_1 = require("@op-engineering/op-sqlcipher");
const chai_1 = __importDefault(require("chai"));
const MochaRNAdapter_1 = require("./MochaRNAdapter");
const react_native_1 = require("react-native");
let expect = chai_1.default.expect;
function dbSetupTests() {
    (0, MochaRNAdapter_1.describe)('DB setup tests', () => {
        (0, MochaRNAdapter_1.it)('Create in memory DB', async () => {
            let inMemoryDb = (0, op_sqlcipher_1.open)({
                name: 'cipherInMemoryTest',
                encryptionKey: 'quack',
                location: ':memory:',
            });
            inMemoryDb.execute('DROP TABLE IF EXISTS User;');
            inMemoryDb.execute('CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;');
            inMemoryDb.close();
        });
        if (react_native_1.Platform.OS === 'android') {
            (0, MochaRNAdapter_1.it)('Create db in external directory Android', () => {
                let androidDb = (0, op_sqlcipher_1.open)({
                    name: 'AndroidSDCardDB',
                    location: op_sqlcipher_1.ANDROID_EXTERNAL_FILES_PATH,
                    encryptionKey: 'quack',
                });
                androidDb.execute('DROP TABLE IF EXISTS User;');
                androidDb.execute('CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;');
                androidDb.close();
            });
        }
        // it('Should fail creating in-memory with non-bool arg', async () => {
        //   try {
        //     open({
        //       name: 'inMemoryTest',
        //     });
        //     expect.fail('Should throw');
        //   } catch (e) {
        //     expect(!!e).to.equal(true);
        //   }
        // });
    });
}
exports.dbSetupTests = dbSetupTests;
