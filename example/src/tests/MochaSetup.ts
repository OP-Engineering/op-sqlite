import 'mocha';
import type * as MochaTypes from 'mocha';
import {clearTests, rootSuite} from './MochaRNAdapter';

export async function runTests(...registrators: Array<() => void>) {
  const promise = new Promise(resolve => {
    const {
      EVENT_RUN_BEGIN,
      EVENT_RUN_END,
      EVENT_TEST_FAIL,
      EVENT_TEST_PASS,
      EVENT_SUITE_BEGIN,
      // EVENT_SUITE_END,
    } = Mocha.Runner.constants;

    clearTests();
    const results: any[] = [];
    var runner = new Mocha.Runner(rootSuite) as MochaTypes.Runner;

    runner
      .once(EVENT_RUN_BEGIN, () => {})
      .on(EVENT_SUITE_BEGIN, (suite: MochaTypes.Suite) => {
        const name = suite.title;
        if (name !== '') {
          results.push({
            description: name,
            key: Math.random().toString(),
            type: 'grouping',
          });
        }
      })
      .on(EVENT_TEST_PASS, (test: MochaTypes.Runnable) => {
        results.push({
          description: test.title,
          key: Math.random().toString(),
          type: 'correct',
        });
      })
      .on(EVENT_TEST_FAIL, (test: MochaTypes.Runnable, err: Error) => {
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
