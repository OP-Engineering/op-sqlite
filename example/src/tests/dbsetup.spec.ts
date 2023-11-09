import {open} from '@op-engineering/op-sqlite';
import chai from 'chai';
import {describe, it} from './MochaRNAdapter';

let expect = chai.expect;

export function dbSetupTests() {
  describe('Setting up a DB', () => {
    it('Create in memory DB', async () => {
      let inMemoryDb = open({
        name: 'inMemoryTest',
        inMemory: true,
      });

      inMemoryDb.execute('DROP TABLE IF EXISTS User;');
      inMemoryDb.execute(
        'CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;',
      );

      inMemoryDb.close();
    });

    it('Should fail creating in-memory with non-bool arg', async () => {
      try {
        open({
          name: 'inMemoryTest',
          // @ts-ignore
          inMemory: 'blah',
        });
        expect.fail('Should throw');
      } catch (e) {
        expect(!!e).to.equal(true);
      }
    });
  });
}
