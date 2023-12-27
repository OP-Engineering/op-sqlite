"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.beforeAll = exports.beforeEach = exports.describe = exports.itOnly = exports.it = exports.clearTests = exports.rootSuite = void 0;
require("mocha");
exports.rootSuite = new Mocha.Suite('');
exports.rootSuite.timeout(10 * 1000);
let mochaContext = exports.rootSuite;
let only = false;
const clearTests = () => {
    exports.rootSuite.suites = [];
    exports.rootSuite.tests = [];
    mochaContext = exports.rootSuite;
    only = false;
};
exports.clearTests = clearTests;
const it = (name, f) => {
    if (!only) {
        const test = new Mocha.Test(name, f);
        mochaContext.addTest(test);
    }
};
exports.it = it;
const itOnly = (name, f) => {
    (0, exports.clearTests)();
    const test = new Mocha.Test(name, f);
    mochaContext.addTest(test);
    only = true;
};
exports.itOnly = itOnly;
const describe = (name, f) => {
    const prevMochaContext = mochaContext;
    mochaContext = new Mocha.Suite(name, prevMochaContext.ctx);
    prevMochaContext.addSuite(mochaContext);
    f();
    mochaContext = prevMochaContext;
};
exports.describe = describe;
const beforeEach = (f) => {
    mochaContext.beforeEach(f);
};
exports.beforeEach = beforeEach;
const beforeAll = (f) => {
    mochaContext.beforeAll(f);
};
exports.beforeAll = beforeAll;
