import {ANDROID_EXTERNAL_FILES_PATH, open} from '@op-engineering/op-sqlite';
import chai from 'chai';
import {describe, it} from './MochaRNAdapter';
import {Platform} from 'react-native';

let expect = chai.expect;

export function dbSetupTests() {
  describe('DB setup tests', () => {
    it('Create in memory DB', async () => {
      let inMemoryDb = open({
        name: 'inMemoryTest',
        location: ':memory:',
      });

      inMemoryDb.execute('DROP TABLE IF EXISTS User;');
      inMemoryDb.execute(
        'CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;',
      );

      inMemoryDb.close();
    });

    if (Platform.OS === 'android') {
      it('Create db in external directory Android', () => {
        let androidDb = open({
          name: 'AndroidSDCardDB',
          location: ANDROID_EXTERNAL_FILES_PATH,
        });
        androidDb.execute('DROP TABLE IF EXISTS User;');
        androidDb.execute(
          'CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;',
        );

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
