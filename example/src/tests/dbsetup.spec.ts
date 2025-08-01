import {
  ANDROID_DATABASE_PATH,
  ANDROID_EXTERNAL_FILES_PATH,
  IOS_LIBRARY_PATH,
  isIOSEmbeeded,
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
  ? '3.49.2'
  : '3.50.0';
const flavor = isLibsql() ? 'libsql' : isSQLCipher() ? 'sqlcipher' : 'sqlite';

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

    // Using the embedded version, you can never be sure which version is used
    // It will change from OS version to version
    if (!isIOSEmbeeded()) {
      it(`Should match the sqlite flavor ${flavor} expected version ${expectedVersion}`, async () => {
        let db = open({
          name: 'versionTest.sqlite',
          encryptionKey: 'test',
        });

        const res = await db.execute('select sqlite_version();');

        expect(res.rows[0]!['sqlite_version()']).to.equal(expectedVersion);
        db.close();
      });
    }

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

        androidDb.close();
      });

      it('Creates db in external nested directory on Android', async () => {
        let androidDb = open({
          name: 'AndroidSDCardDB.sqlite',
          location: `${ANDROID_EXTERNAL_FILES_PATH}/nested`,
          encryptionKey: 'test',
        });

        await androidDb.execute('DROP TABLE IF EXISTS User;');
        await androidDb.execute(
          'CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;',
        );

        androidDb.close();
      });
    }

    // Currently this only tests the function is there
    it('Should load extension', async () => {
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
        db.delete();
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
      let location =
        Platform.OS === 'ios' ? IOS_LIBRARY_PATH : ANDROID_DATABASE_PATH;
      let db = open({
        name: 'deleteTest',
        encryptionKey: 'test',
        location,
      });

      expect(db.getDbPath()).to.contain(location);

      db.delete();
    });

    it('Should create db in custom folder', async () => {
      let db = open({
        name: 'customFolderTest.sqlite',
        encryptionKey: 'test',
        location: 'myFolder',
      });

      let path = db.getDbPath();
      expect(path).to.contain('myFolder');
      db.delete();
    });

    it('Should create nested folders', async () => {
      let db = open({
        name: 'nestedFolderTest.sqlite',
        encryptionKey: 'test',
        location: 'myFolder/nested',
      });

      let path = db.getDbPath();
      expect(path).to.contain('myFolder/nested');
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

      let db = open({
        name: 'sample2.sqlite',
        encryptionKey: 'test',
        location: 'sqlite',
      });

      let path = db.getDbPath();
      expect(path).to.contain('sqlite/sample2.sqlite');
      db.delete();
    });

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

    db.attach({
      secondaryDbFileName: 'attachTest2.sqlite',
      alias: 'attach2',
    });

    db.executeSync('DROP TABLE IF EXISTS attach2.test;');
    db.executeSync(
      'CREATE TABLE IF NOT EXISTS attach2.test (id INTEGER PRIMARY KEY);',
    );
    let res = db.executeSync('INSERT INTO attach2.test (id) VALUES (1);');
    expect(res).to.exist;

    db.detach('attach2');

    db.delete();

    db2 = open({
      name: 'attachTest2.sqlite',
      encryptionKey: 'test',
    });
    db2.delete();
  });

  it('Can get db path', () => {
    let db = open({
      name: 'pathTest.sqlite',
      encryptionKey: 'test',
    });

    let path = db.getDbPath();
    expect(path).to.exist;
    db.close();
  });

  if (isSQLCipher()) {
    it('Can open SQLCipher db without encryption key', () => {
      let db = open({
        name: 'pathTest.sqlite',
      });

      db.close();
    });
  }
}
