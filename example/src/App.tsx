import {
  IOS_DOCUMENT_PATH,
  open,
  openV2,
} from '@op-engineering/op-sqlite';
import { useEffect, useState } from 'react';
import {
  SafeAreaView,
  ScrollView,
  Text,
  View,
} from 'react-native';
const ROWS = 100_000;

export default function App() {
  const [perfMetrics, setPerfMetrics] = useState<{
    openV2Time: number;
    openTime: number;
    newDbInsertTime: number;
    oldDbInsertTime: number;
  } | null>(null);

  useEffect(() => {
    setTimeout(async () => {
      // Warm-up phase: Initialize the native SQLite module to avoid cold-start skew
      // This ensures both `open` and `openV2` measurements start from a "warm" state
      const warmupDb = open({ name: 'warmup.sqlite' });
      warmupDb.executeSync('SELECT 1'); // Trigger actual initialization
      warmupDb.close();

      let start = 0;
      let end = 0;

      start = performance.now();
      const oldDb = open({
        name: 'olddb.sqlite',
      });
      end = performance.now();
      const openTime = end - start;

      start = performance.now();
      const newDb = openV2({
        path: `${IOS_DOCUMENT_PATH}/newdb.sqlite`,
      });
      end = performance.now();
      const openV2Time = end - start;

      // Measure opening oldDb

      // Measure create table + bulk inserts
      start = performance.now();

      // Create table
      newDb.executeSync('DROP TABLE IF EXISTS performance_test');
      newDb.executeSync(
        'CREATE TABLE performance_test (id INTEGER PRIMARY KEY, name TEXT, value INTEGER, timestamp REAL)',
      );

      // Insert 1000 rows
      newDb.executeSync('BEGIN TRANSACTION');
      for (let i = 0; i < ROWS; i++) {
        newDb.executeSync(
          'INSERT INTO performance_test (name, value, timestamp) VALUES (?, ?, ?)',
          [`Test ${i}`, i * 10, Date.now()],
        );
      }
      newDb.executeSync('COMMIT');

      end = performance.now();
      const newDbInsertTime = end - start;

      // Verify data was inserted
      // const result = newDb.executeSync(
      //   'SELECT COUNT(*) as count FROM performance_test',
      // );
      // console.log(
      //   `[Performance] Total rows inserted (newDb): ${result.rows?._array[0]?.count}`,
      // );

      // Measure create table + bulk inserts for oldDb
      start = performance.now();

      // Create table
      oldDb.executeSync('DROP TABLE IF EXISTS performance_test');
      oldDb.executeSync(
        'CREATE TABLE performance_test (id INTEGER PRIMARY KEY, name TEXT, value INTEGER, timestamp REAL)',
      );

      oldDb.executeSync('BEGIN TRANSACTION');
      for (let i = 0; i < ROWS; i++) {
        oldDb.executeSync(
          'INSERT INTO performance_test (name, value, timestamp) VALUES (?, ?, ?)',
          [`Test ${i}`, i * 10, Date.now()],
        );
      }
      oldDb.executeSync('COMMIT');

      end = performance.now();
      const oldDbInsertTime = end - start;

      // Do a simple async execute query
      // try {
      //   // await newDb.execute(
      //   //   'INSERT INTO performance_test (name, value, timestamp) VALUES (?, ?, ?)',
      //   //   [`Test -1`, 10, Date.now()],
      //   // );
      //   let res = await newDb.execute(
      //     "SELECT * FROM performance_test LIMIT 10"
      //   )
      //   console.log(res)

      // } catch (e) {
      //   console.error(e)
      // }

      setPerfMetrics({
        openV2Time,
        openTime,
        newDbInsertTime,
        oldDbInsertTime,
      });
    }, 1000);
  }, []);

  // let passingTests = 0;
  // let failingTests = 0;

  // const allTestsPassed = results.reduce((acc: boolean, r: any) => {
  //   if (r.type === 'incorrect') {
  //     failingTests++;
  //   } else if (r.type === 'correct') {
  //     passingTests++;
  //   }

  //   return acc && r.type !== 'incorrect';
  // }, true);

  // const shareDb = async () => {
  //   try {
  //     const db = open({
  //       name: 'shareableDb.sqlite',
  //     });

  //     await db.execute(
  //       'CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY, name TEXT)',
  //     );
  //     await db.execute("INSERT INTO test (name) VALUES ('test')");
  //     const res = await db.execute('SELECT * FROM test');
  //     console.log(res);

  //     await db.close();
  //     await Share.open({
  //       url: 'file://' + db.getDbPath(),
  //       failOnCancel: false,
  //       type: 'application/x-sqlite3',
  //     });
  //   } catch (e) {
  //     console.log(e);
  //   }
  // };

  // const createLargeDb = async () => {
  //   await createLargeDB();
  // };

  // const queryLargeDb = async () => {
  //   try {
  //     const times = await queryLargeDB();
  //     setTimes(times.loadFromDb);
  //     setAccessingTimes(times.access);
  //     setPrepareTimes(times.prepare);
  //     setPrepareExecutionTimes(times.preparedExecution);
  //     setRawExecutionTimes(times.rawExecution);
  //   } catch (e) {
  //     console.error(e);
  //   } finally {
  //   }
  // };

  // const copyDbPathToClipboad = async () => {
  //   const db = await open({ name: 'shareableDb.sqlite' });
  //   const path = db.getDbPath();
  //   await db.close();
  //   console.warn(path);
  //   Clipboard.setString(path);
  // };

  // const queryAndReload = async () => {
  //   queryLargeDB();
  //   setTimeout(() => {
  //     RNRestart.restart();
  //   }, 200);
  // };

  // const loadExtention = async () => {
  //   try {
  //     let db = open({ name: 'extension.sqlite' });
  //     let path = 'libcrsqlite';
  //     if (Platform.OS === 'ios') {
  //       console.log('Getting new path');
  //       path = getDylibPath('io.vlcn.crsqlite', 'crsqlite');
  //     }
  //     console.log(`final path ${path}`);
  //     db.loadExtension(path);
  //     console.log('Loaded extension!');
  //   } catch (e) {
  //     console.error(e);
  //   }
  // };

  return (
    <SafeAreaView className="flex-1 bg-neutral-900">
      <ScrollView>
        {/*<Button title="Reload app middle of query" onPress={queryAndReload} />
        <Button title="Load extension" onPress={loadExtention} />
        <Button title="Share DB" onPress={shareDb} />
        <Button title="Copy DB Path" onPress={copyDbPathToClipboad} />
        <Button title="Create 300k Record DB" onPress={createLargeDb} />
        <Button title="Query 300k Records" onPress={queryLargeDb} />*/}

        {perfMetrics && (
          <View className="p-4 mt-4 bg-neutral-800 rounded-lg mx-2">
            <Text className="text-xl text-white font-bold mb-3">
              Performance Metrics ({ROWS.toLocaleString()} rows)
            </Text>

            <View className="mb-2">
              <Text className="text-green-400 text-lg">
                🟢 openV2 (NativeState): {perfMetrics.openV2Time.toFixed(2)}ms
              </Text>
            </View>

            <View className="mb-2">
              <Text className="text-blue-400 text-lg">
                🔵 open (HostObject): {perfMetrics.openTime.toFixed(2)}ms
              </Text>
            </View>

            <View className="mb-2">
              <Text className="text-green-400 text-lg">
                🟢 Create + Insert (NativeState):{' '}
                {perfMetrics.newDbInsertTime.toFixed(2)}ms
              </Text>
            </View>

            <View className="mb-2">
              <Text className="text-blue-400 text-lg">
                🔵 Create + Insert (HostObject):{' '}
                {perfMetrics.oldDbInsertTime.toFixed(2)}ms
              </Text>
            </View>

            <View className="mt-3 pt-3 border-t border-neutral-700">
              <Text className="text-yellow-400 text-sm">
                Open speedup:{' '}
                {(perfMetrics.openTime / perfMetrics.openV2Time).toFixed(2)}x
              </Text>
              <Text className="text-yellow-400 text-sm">
                Insert speedup:{' '}
                {(
                  perfMetrics.oldDbInsertTime / perfMetrics.newDbInsertTime
                ).toFixed(2)}
                x
              </Text>
            </View>
          </View>
        )}
        {/*{!!times.length && (
          <Text className="text-lg text-white self-center">
            Normal query{' '}
            {(times.reduce((acc, t) => (acc += t), 0) / times.length).toFixed(
              0,
            )}{' '}
            ms
          </Text>
        )}
        {!!accessingTimes.length && (
          <Text className="text-lg text-white self-center">
            Read property{' '}
            {(
              accessingTimes.reduce((acc, t) => (acc += t), 0) /
              accessingTimes.length
            ).toFixed(0)}{' '}
            ms
          </Text>
        )}
        {!!prepareTimes.length && (
          <Text className="text-lg text-white self-center">
            Prepare statement{' '}
            {(
              prepareTimes.reduce((acc, t) => (acc += t), 0) /
              prepareTimes.length
            ).toFixed(0)}{' '}
            ms
          </Text>
        )}
        {!!prepareExecutionTimes.length && (
          <Text className="text-lg text-white self-center">
            Execute prepared query{' '}
            {(
              prepareExecutionTimes.reduce((acc, t) => (acc += t), 0) /
              prepareExecutionTimes.length
            ).toFixed(0)}{' '}
            ms
          </Text>
        )}
        {!!rawExecutionTimes.length && (
          <Text className="text-lg text-white self-center">
            Raw execution:{' '}
            {(
              rawExecutionTimes.reduce((acc, t) => (acc += t), 0) /
              rawExecutionTimes.length
            ).toFixed(0)}{' '}
            ms
          </Text>
        )}
        <View className="p-2">
          <Text className="text-white">
            is libsql: {isLibsql() ? 'YES' : 'NO'}
          </Text>
        </View>
        <Text
          className={clsx('font-bold flex-1 text-white p-2 mt-4', {
            'bg-green-500': allTestsPassed,
            'bg-red-500': !allTestsPassed,
          })}
        >
          Test Suite {passingTests}/{passingTests + failingTests}
        </Text>
        {results
          .filter((t: any) => t.type !== 'grouping')
          .sort((a: any, b: any) => {
            if (a.type === 'incorrect') {
              return -1;
            }

            if (b.type === 'incorrect') {
              return 1;
            }

            return 0;
          })
          .map((r: any, i: number) => {
            if (r.type === 'grouping') {
              return (
                <Text className="p-2 font-semibold text-white" key={i}>
                  {r.description}
                </Text>
              );
            }

            if (r.type === 'incorrect') {
              return (
                <View
                  className="border-b border-neutral-800 p-2 flex-row"
                  key={i}
                >
                  <Text className="text-red-500 flex-1">
                    {r.description}: {r.errorMsg}
                  </Text>
                </View>
              );
            }

            return (
              <View
                className="border-b border-neutral-800 p-2 flex-row"
                key={i}
              >
                <Text className="text-green-500 flex-1">{r.description}</Text>
              </View>
            );
          })}*/}
      </ScrollView>
    </SafeAreaView>
  );
}
