"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.runTests = void 0;
require("mocha");
const MochaRNAdapter_1 = require("./MochaRNAdapter");
async function runTests(...registrators) {
    const promise = new Promise(resolve => {
        const { EVENT_RUN_BEGIN, EVENT_RUN_END, EVENT_TEST_FAIL, EVENT_TEST_PASS, EVENT_SUITE_BEGIN, EVENT_SUITE_END, } = Mocha.Runner.constants;
        (0, MochaRNAdapter_1.clearTests)();
        const results = [];
        var runner = new Mocha.Runner(MochaRNAdapter_1.rootSuite);
        runner
            .once(EVENT_RUN_BEGIN, () => { })
            .on(EVENT_SUITE_BEGIN, (suite) => {
            const name = suite.title;
            if (name !== '') {
                results.push({
                    description: name,
                    key: Math.random().toString(),
                    type: 'grouping',
                });
            }
        })
            .on(EVENT_TEST_PASS, (test) => {
            results.push({
                description: test.title,
                key: Math.random().toString(),
                type: 'correct',
            });
        })
            .on(EVENT_TEST_FAIL, (test, err) => {
            results.push({
                description: test.title,
                key: Math.random().toString(),
                type: 'incorrect',
                errorMsg: err.message,
            });
        })
            .once(EVENT_RUN_END, () => {
            resolve(results);
        });
        registrators.forEach(register => {
            register();
        });
        runner.run();
    });
    return promise;
}
exports.runTests = runTests;
