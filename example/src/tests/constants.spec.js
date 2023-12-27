"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.constantsTests = void 0;
const op_sqlcipher_1 = require("@op-engineering/op-sqlcipher");
const chai_1 = __importDefault(require("chai"));
const MochaRNAdapter_1 = require("./MochaRNAdapter");
const react_native_1 = require("react-native");
let expect = chai_1.default.expect;
function constantsTests() {
    (0, MochaRNAdapter_1.describe)('Constants tests', () => {
        (0, MochaRNAdapter_1.it)('Constants are there', async () => {
            if (react_native_1.Platform.OS === 'ios') {
                expect(op_sqlcipher_1.IOS_DOCUMENT_PATH).to.exist;
                expect(op_sqlcipher_1.IOS_LIBRARY_PATH).to.exist;
            }
            else {
                expect(op_sqlcipher_1.ANDROID_EXTERNAL_FILES_PATH).to.exist;
                expect(op_sqlcipher_1.ANDROID_DATABASE_PATH).to.exist;
                expect(op_sqlcipher_1.ANDROID_FILES_PATH).to.exist;
            }
        });
    });
}
exports.constantsTests = constantsTests;
