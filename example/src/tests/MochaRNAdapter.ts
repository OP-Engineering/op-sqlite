import 'mocha';
import type * as MochaTypes from 'mocha';

export let rootSuite = new Mocha.Suite('') as MochaTypes.Suite;
rootSuite.timeout(10 * 1000);
// rootSuite.timeout(60 * 60 * 1000);

let mochaContext = rootSuite;
let only = false;

export const clearTests = () => {
  rootSuite.suites = [];
  rootSuite.tests = [];
  rootSuite = new Mocha.Suite('') as MochaTypes.Suite;
  mochaContext = rootSuite;
  only = false;
};

export const it = (
  name: string,
  f: MochaTypes.Func | MochaTypes.AsyncFunc,
): void => {
  if (!only) {
    const test = new Mocha.Test(name, f);
    mochaContext.addTest(test);
  }
};

export const itOnly = (
  name: string,
  f: MochaTypes.Func | MochaTypes.AsyncFunc,
): void => {
  clearTests();
  const test = new Mocha.Test(name, f);
  mochaContext.addTest(test);
  only = true;
};

export const describe = (name: string, f: () => void): void => {
  const prevMochaContext = mochaContext;
  mochaContext = new Mocha.Suite(
    name,
    prevMochaContext.ctx,
  ) as MochaTypes.Suite;
  prevMochaContext.addSuite(mochaContext);
  f();
  mochaContext = prevMochaContext;
};

export const beforeEach = (f: () => void): void => {
  mochaContext.beforeEach(f);
};

export const afterEach = (f: () => void): void => {
  mochaContext.afterEach(f);
};

export const beforeAll = (f: any) => {
  mochaContext.beforeAll(f);
};

export const afterAll = (f: any) => {
  mochaContext.afterAll(f);
};
