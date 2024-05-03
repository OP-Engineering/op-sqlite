import {
  ANDROID_DATABASE_PATH,
  ANDROID_EXTERNAL_FILES_PATH,
  IOS_LIBRARY_PATH,
  isSQLCipher,
  open,
} from '@op-engineering/op-sqlite';
import chai from 'chai';
import {describe, it} from './MochaRNAdapter';
import {Platform} from 'react-native';

let expect = chai.expect;

const expectedVersion = isSQLCipher() ? '3.44.2' : '3.45.1';

export function dbSetupTests() {
  describe('DB setup tests', () => {
    it(`Should match the sqlite expected version ${expectedVersion}`, async () => {
      let db = open({
        name: 'versionTest.sqlite',
        encryptionKey: 'test',
      });

      const res = db.execute('select sqlite_version();');

      expect(res.rows?._array[0]['sqlite_version()']).to.equal(expectedVersion);
      db.close();
    });

    it('Create in memory DB', async () => {
      let inMemoryDb = open({
        name: 'inMemoryTest.sqlite',
        location: ':memory:',
        encryptionKey: 'test',
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
          name: 'AndroidSDCardDB.sqlite',
          location: ANDROID_EXTERNAL_FILES_PATH,
          encryptionKey: 'test',
        });
        androidDb.execute('DROP TABLE IF EXISTS User;');
        androidDb.execute(
          'CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;',
        );

        androidDb.close();
      });
    }

    it('Should load extension on runtime', async () => {
      let db = open({
        name: 'extensionDb',
        encryptionKey: 'test',
      });
      try {
        db.loadExtension('path');
      } catch (e) {
        // TODO load a sample extension
        expect(e).to.exist;
      }
    });

    it('Should delete db', async () => {
      let db = open({
        name: 'deleteTest',
        encryptionKey: 'test',
      });

      db.delete();
    });

    it('Should delete db with absolute path', async () => {
      let db = open({
        name: 'deleteTest',
        encryptionKey: 'test',
        location:
          Platform.OS === 'ios' ? IOS_LIBRARY_PATH : ANDROID_DATABASE_PATH,
      });

      db.delete();
    });

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
