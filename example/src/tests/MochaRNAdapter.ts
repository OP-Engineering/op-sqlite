import 'mocha';
import type * as MochaTypes from 'mocha';

const TIMEOUT = 10 * 1000;

export const rootSuite = new Mocha.Suite('') as MochaTypes.Suite;
rootSuite.timeout(TIMEOUT);
let suite = new Mocha.Suite('') as MochaTypes.Suite;
let only = false;

export const clearTests = () => {
  suite.suites = [];
  suite.tests = [];
  rootSuite.suites = [];
  rootSuite.tests = [];
  only = false;
};

export const it = (
  name: string,
  f: MochaTypes.Func | MochaTypes.AsyncFunc,
): void => {
  if (!only) {
    const test = new Mocha.Test(name, f);
    suite.addTest(test);
  }
};

export const itOnly = (
  name: string,
  f: MochaTypes.Func | MochaTypes.AsyncFunc,
): void => {
  clearTests();
  const test = new Mocha.Test(name, f);
  suite.addTest(test);
  rootSuite.addSuite(suite);
  only = true;
};

export const describe = (name: string, f: () => void): void => {
  suite = new Mocha.Suite(name) as MochaTypes.Suite;
  rootSuite.addSuite(suite);
  f();
};

export const beforeEach = (f: () => void): void => {
  suite.beforeEach(f);
};

export const afterEach = (f: () => void): void => {
  suite.afterEach(f);
};

export const beforeAll = (f: any) => {
  suite.beforeAll(f);
};

export const afterAll = (f: any) => {
  suite.afterAll(f);
};
