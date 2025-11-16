import {
  ANDROID_DATABASE_PATH,
  IOS_DOCUMENT_PATH,
  open,
  openV2,
} from '@op-engineering/op-sqlite';
import {useEffect, useState} from 'react';
import {Platform, Text, View} from 'react-native';
import {SafeAreaView} from 'react-native-safe-area-context';

const ROWS = 100_000;

export default function App() {
  const [perfMetrics, setPerfMetrics] = useState<{
    newDbInsertTime: number;
    oldDbInsertTime: number;
    newDbInsertTimeSync: number;
    oldDbInsertTimeSync: number;
  } | null>(null);

  useEffect(() => {
    setTimeout(async () => {
      try {
        const warmupDb = open({name: 'warmup.sqlite'});
        warmupDb.executeSync('SELECT 1');
        warmupDb.close();

        let start = 0;
        let end = 0;

        const oldDb = open({name: 'olddb.sqlite'});

        start = performance.now();
        await oldDb.execute('DROP TABLE IF EXISTS performance_test');
        await oldDb.execute(
          'CREATE TABLE performance_test (id INTEGER PRIMARY KEY, name TEXT, value INTEGER, timestamp REAL)',
        );
        await oldDb.execute('BEGIN TRANSACTION');
        for (let i = 0; i < ROWS; i++) {
          await oldDb.execute(
            'INSERT INTO performance_test (name, value, timestamp) VALUES (?, ?, ?)',
            [`Test ${i}`, i * 10, 123456789],
          );
        }
        await oldDb.execute('COMMIT');
        end = performance.now();

        const oldDbInsertTime = end - start;

        const newDb = openV2({
          path:
            Platform.OS === 'ios'
              ? `${IOS_DOCUMENT_PATH}/newdb.sqlite`
              : `${ANDROID_DATABASE_PATH}/newdb.sqlite`,
        });

        start = performance.now();
        await newDb.execute('DROP TABLE IF EXISTS performance_test');
        await newDb.execute(
          'CREATE TABLE performance_test (id INTEGER PRIMARY KEY, name TEXT, value INTEGER, timestamp REAL)',
        );
        await newDb.execute('BEGIN TRANSACTION');
        for (let i = 0; i < ROWS; i++) {
          await newDb.execute(
            'INSERT INTO performance_test (name, value, timestamp) VALUES (?, ?, ?)',
            [`Test ${i}`, i * 10, 123456789],
          );
        }
        await newDb.execute('COMMIT');
        end = performance.now();

        const newDbInsertTime = end - start;

        const oldDbSync = open({name: 'olddb-sync.sqlite'});
        start = performance.now();
        oldDbSync.executeSync('DROP TABLE IF EXISTS performance_test_sync');
        oldDbSync.executeSync(
          'CREATE TABLE performance_test_sync (id INTEGER PRIMARY KEY, name TEXT, value INTEGER, timestamp REAL)',
        );
        oldDbSync.executeSync('BEGIN TRANSACTION');
        for (let i = 0; i < ROWS; i++) {
          oldDbSync.executeSync(
            'INSERT INTO performance_test_sync (name, value, timestamp) VALUES (?, ?, ?)',
            [`Test ${i}`, i * 10, 123456789],
          );
        }
        oldDbSync.executeSync('COMMIT');
        end = performance.now();
        const oldDbInsertTimeSync = end - start;

        const newDbSync = openV2({
          path:
            Platform.OS === 'ios'
              ? `${IOS_DOCUMENT_PATH}/newdb-sync.sqlite`
              : `${ANDROID_DATABASE_PATH}/newdb-sync.sqlite`,
        });
        start = performance.now();
        newDbSync.executeSync('DROP TABLE IF EXISTS performance_test_sync');
        newDbSync.executeSync(
          'CREATE TABLE performance_test_sync (id INTEGER PRIMARY KEY, name TEXT, value INTEGER, timestamp REAL)',
        );
        newDbSync.executeSync('BEGIN TRANSACTION');
        for (let i = 0; i < ROWS; i++) {
          newDbSync.executeSync(
            'INSERT INTO performance_test_sync (name, value, timestamp) VALUES (?, ?, ?)',
            [`Test ${i}`, i * 10, 123456789],
          );
        }
        newDbSync.executeSync('COMMIT');
        end = performance.now();
        const newDbInsertTimeSync = end - start;

        setPerfMetrics({
          newDbInsertTime,
          oldDbInsertTime,
          newDbInsertTimeSync,
          oldDbInsertTimeSync,
        });
      } catch (e) {
        console.error(`Error on queries ${e}`);
      }
    }, 0);
  }, []);

  return (
    <SafeAreaView className="h-full bg-neutral-900">
      <View className="flex-1 bg-neutral-900 mt-32">
        {!perfMetrics && (
          <View className="p-4 mt-4 bg-neutral-800 rounded-lg mx-2">
            <Text className="text-xl text-white font-bold mb-3">
              Running...
            </Text>
          </View>
        )}
        {perfMetrics && (
          <View className="p-4 mt-4 bg-neutral-800 rounded-lg mx-2">
            <Text className="text-xl text-white font-bold mb-3">
              Performance Metrics ({ROWS.toLocaleString()} rows)
            </Text>

            <View className="mb-2">
              <Text className="text-blue-400 ">
                Old Async: {perfMetrics.oldDbInsertTime.toFixed(2)}ms
              </Text>
              <Text className="text-blue-300">
                Old Sync: {perfMetrics.oldDbInsertTimeSync.toFixed(2)}ms
              </Text>
            </View>

            <View className="mb-2">
              <Text className="text-green-400">
                New Async: {perfMetrics.newDbInsertTime.toFixed(2)}ms
              </Text>
              <Text className="text-green-300">
                New Sync: {perfMetrics.newDbInsertTimeSync.toFixed(2)}ms
              </Text>
            </View>

            <View className="mt-3 pt-3 border-t border-neutral-700">
              <Text className="text-yellow-400 text-sm">
                Async speedup:{' '}
                {(
                  perfMetrics.oldDbInsertTime / perfMetrics.newDbInsertTime
                ).toFixed(2)}
                x
              </Text>
              <Text className="text-yellow-300 text-sm mt-1">
                Sync speedup:{' '}
                {(
                  perfMetrics.oldDbInsertTimeSync /
                  perfMetrics.newDbInsertTimeSync
                ).toFixed(2)}
                x
              </Text>
            </View>
          </View>
        )}
      </View>
    </SafeAreaView>
  );
}
