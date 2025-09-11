import { Platform } from 'react-native';
import {
  open,
  WINDOWS_LOCAL_DATA_PATH,
  WINDOWS_TEMP_PATH,
  WINDOWS_ROAMING_DATA_PATH,
  moveAssetsDatabase,
} from '@op-engineering/op-sqlite';

export async function windowsTests() {
  if (Platform.OS !== 'windows') {
    console.log('Skipping Windows tests on non-Windows platform');
    return;
  }

  console.log('Running Windows-specific tests...');

  // Test 1: Open database in local data path
  console.log('Test 1: Opening database in local data path');
  const localDb = open({
    name: 'local_test.db',
    location: WINDOWS_LOCAL_DATA_PATH,
  });

  await localDb.execute(`
    CREATE TABLE IF NOT EXISTS test_local (
      id INTEGER PRIMARY KEY,
      data TEXT
    )
  `);

  await localDb.execute('INSERT INTO test_local (data) VALUES (?)', [
    'Local data test',
  ]);

  const localResult = await localDb.execute('SELECT * FROM test_local');
  console.log('Local DB result:', localResult.rows);
  localDb.close();

  // Test 2: Open database in temp path
  console.log('Test 2: Opening database in temp path');
  const tempDb = open({
    name: 'temp_test.db',
    location: WINDOWS_TEMP_PATH,
  });

  await tempDb.execute(`
    CREATE TABLE IF NOT EXISTS test_temp (
      id INTEGER PRIMARY KEY,
      data TEXT
    )
  `);

  await tempDb.execute('INSERT INTO test_temp (data) VALUES (?)', [
    'Temp data test',
  ]);

  const tempResult = await tempDb.execute('SELECT * FROM test_temp');
  console.log('Temp DB result:', tempResult.rows);
  tempDb.close();

  // Test 3: Open database in roaming path
  console.log('Test 3: Opening database in roaming path');
  const roamingDb = open({
    name: 'roaming_test.db',
    location: WINDOWS_ROAMING_DATA_PATH,
  });

  await roamingDb.execute(`
    CREATE TABLE IF NOT EXISTS test_roaming (
      id INTEGER PRIMARY KEY,
      data TEXT
    )
  `);

  await roamingDb.execute('INSERT INTO test_roaming (data) VALUES (?)', [
    'Roaming data test',
  ]);

  const roamingResult = await roamingDb.execute('SELECT * FROM test_roaming');
  console.log('Roaming DB result:', roamingResult.rows);
  roamingDb.close();

  // Test 4: In-memory database
  console.log('Test 4: In-memory database');
  const memDb = open({
    name: ':memory:',
  });

  await memDb.execute(`
    CREATE TABLE test_memory (
      id INTEGER PRIMARY KEY,
      value REAL
    )
  `);

  await memDb.execute('INSERT INTO test_memory (value) VALUES (?), (?)', [
    3.14,
    2.71,
  ]);

  const memResult = await memDb.execute('SELECT SUM(value) as total FROM test_memory');
  console.log('Memory DB sum:', memResult.rows[0].total);
  memDb.close();

  // Test 5: Batch operations
  console.log('Test 5: Batch operations');
  const batchDb = open({
    name: 'batch_test.db',
    location: WINDOWS_LOCAL_DATA_PATH,
  });

  await batchDb.execute(`
    CREATE TABLE IF NOT EXISTS batch_table (
      id INTEGER PRIMARY KEY,
      name TEXT
    )
  `);

  const batchResult = await batchDb.executeBatch([
    ['INSERT INTO batch_table (name) VALUES (?)', ['Item 1']],
    ['INSERT INTO batch_table (name) VALUES (?)', ['Item 2']],
    ['INSERT INTO batch_table (name) VALUES (?)', ['Item 3']],
  ]);

  console.log('Batch insert rows affected:', batchResult.rowsAffected);

  const batchQuery = await batchDb.execute('SELECT COUNT(*) as count FROM batch_table');
  console.log('Batch table count:', batchQuery.rows[0].count);
  batchDb.close();

  // Test 6: Prepared statements
  console.log('Test 6: Prepared statements');
  const prepDb = open({
    name: 'prepared_test.db',
    location: WINDOWS_LOCAL_DATA_PATH,
  });

  await prepDb.execute(`
    CREATE TABLE IF NOT EXISTS prepared_table (
      id INTEGER PRIMARY KEY,
      value INTEGER
    )
  `);

  const stmt = prepDb.prepareStatement('INSERT INTO prepared_table (value) VALUES (?)');
  
  for (let i = 0; i < 5; i++) {
    stmt.execute([i * 10]);
  }

  const prepResult = await prepDb.execute('SELECT * FROM prepared_table ORDER BY value');
  console.log('Prepared statement results:', prepResult.rows);
  prepDb.close();

  // Test 7: Move assets database (if available)
  console.log('Test 7: Testing moveAssetsDatabase');
  try {
    const moved = moveAssetsDatabase('sample.sqlite');
    if (moved) {
      console.log('Successfully moved assets database');
      const assetsDb = open({
        name: 'sample.sqlite',
        location: WINDOWS_LOCAL_DATA_PATH,
      });
      // Test if we can query it
      const tables = await assetsDb.execute(
        "SELECT name FROM sqlite_master WHERE type='table'"
      );
      console.log('Tables in assets database:', tables.rows);
      assetsDb.close();
    } else {
      console.log('No assets database to move or already exists');
    }
  } catch (error) {
    console.log('Assets database test skipped:', error.message);
  }

  console.log('Windows tests completed successfully!');
}

// Path validation tests
export function validateWindowsPaths() {
  if (Platform.OS !== 'windows') {
    return;
  }

  console.log('Windows Path Constants:');
  console.log('WINDOWS_LOCAL_DATA_PATH:', WINDOWS_LOCAL_DATA_PATH);
  console.log('WINDOWS_TEMP_PATH:', WINDOWS_TEMP_PATH);
  console.log('WINDOWS_ROAMING_DATA_PATH:', WINDOWS_ROAMING_DATA_PATH);

  // These should be empty on Windows
  console.log('Other platform paths (should be empty):');
  console.log('IOS_DOCUMENT_PATH:', IOS_DOCUMENT_PATH || '(empty)');
  console.log('ANDROID_DATABASE_PATH:', ANDROID_DATABASE_PATH || '(empty)');
}
