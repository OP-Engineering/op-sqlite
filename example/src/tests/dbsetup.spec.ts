import {
  ANDROID_DATABASE_PATH,
  ANDROID_EXTERNAL_FILES_PATH,
  IOS_LIBRARY_PATH,
  isLibsql,
  isSQLCipher,
  moveAssetsDatabase,
  open,
} from '@op-engineering/op-sqlite';
import chai from 'chai';
import {describe, it} from './MochaRNAdapter';
import {Platform} from 'react-native';

let expect = chai.expect;

const expectedVersion = isLibsql()
  ? '3.45.1'
  : isSQLCipher()
  ? '3.46.1'
  : '3.46.1';

// const expectedSqliteVecVersion = 'v0.1.2-alpha.7';

export function dbSetupTests() {
  describe('DB setup tests', () => {
    // it('Should match the sqlite_vec version', async () => {
    //   let db = open({
    //     name: 'versionTest.sqlite',
    //   });

    //   const res = db.execute('select vec_version();');

    //   expect(res.rows?._array[0]['vec_version()']).to.equal(
    //     expectedSqliteVecVersion,
    //   );

    //   db.close();
    // });

    it(`Should match the sqlite expected version ${expectedVersion}`, async () => {
      let db = open({
        name: 'versionTest.sqlite',
        encryptionKey: 'test',
      });

      const res = await db.execute('select sqlite_version();');

      expect(res.rows[0]!['sqlite_version()']).to.equal(expectedVersion);
      db.close();
    });

    it('Create in memory DB', async () => {
      let inMemoryDb = open({
        name: 'inMemoryTest.sqlite',
        location: ':memory:',
        encryptionKey: 'test',
      });

      await inMemoryDb.execute('DROP TABLE IF EXISTS User;');
      await inMemoryDb.execute(
        'CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;',
      );

      inMemoryDb.close();
    });

    if (Platform.OS === 'android') {
      it('Create db in external directory Android', async () => {
        let androidDb = open({
          name: 'AndroidSDCardDB.sqlite',
          location: ANDROID_EXTERNAL_FILES_PATH,
          encryptionKey: 'test',
        });

        await androidDb.execute('DROP TABLE IF EXISTS User;');
        await androidDb.execute(
          'CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;',
        );

        await androidDb.close();
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
      } finally {
        db.close();
        db.delete();
      }
    });

    it('Should delete db', async () => {
      let db = open({
        name: 'deleteTest',
        encryptionKey: 'test',
      });

      db.close();
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

    it('Moves assets database simple', async () => {
      const copied = await moveAssetsDatabase({filename: 'sample.sqlite'});

      expect(copied).to.equal(true);
    });

    it('Moves assets database with path', async () => {
      const copied = await moveAssetsDatabase({
        filename: 'sample2.sqlite',
        path: 'sqlite',
      });

      expect(copied).to.equal(true);
    });

    it('Moves assets database with path and overwrite', async () => {
      const copied = await moveAssetsDatabase({
        filename: 'sample2.sqlite',
        path: 'sqlite',
        overwrite: true,
      });

      expect(copied).to.equal(true);
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

    it('Creates new connections per query and closes them', async () => {
      for (let i = 0; i < 100; i++) {
        let db = open({
          name: 'versionTest.sqlite',
          encryptionKey: 'test',
        });

        await db.execute('select 1;');

        db.close();
      }
    });

    it('Closes connections correctly', async () => {
      try {
        let db1 = open({
          name: 'closeTest.sqlite',
        });
        expect(db1).to.exist;
        open({
          name: 'closeTest.sqlite',
        });
      } catch (e) {
        expect(e).to.exist;
      }
    });
  });

  it('Can attach/dettach database', () => {
    let db = open({
      name: 'attachTest.sqlite',
      encryptionKey: 'test',
    });
    let db2 = open({
      name: 'attachTest2.sqlite',
      encryptionKey: 'test',
    });
    db2.close();
    db.attach('attachTest.sqlite', 'attachTest2.sqlite', 'attach2');
    db.executeSync(
      'CREATE TABLE IF NOT EXISTS attach2.test (id INTEGER PRIMARY KEY);',
    );
    let res = db.executeSync('INSERT INTO attach2.test (id) VALUES (1);');
    expect(res).to.exist;
    db.detach('attachTest2.sqlite', 'attach2');
    db2 = open({
      name: 'attachTest2.sqlite',
      encryptionKey: 'test',
    });
    db2.close();
    db2.delete();
  });
}
