import {useEffect, useState} from 'react';
import {setServerResults, stopServer} from './server';
import {
  displayResults,
  runTests,
  allTestsPassed,
} from '@op-engineering/op-test';
import './tests'; // import all tests to register them
import {SafeAreaProvider, SafeAreaView} from 'react-native-safe-area-context';
import {performanceTest} from './performance_test';
import {StyleSheet, Text, View} from 'react-native';
import {open} from '@op-engineering/op-sqlite';

export default function App() {
  const [results, setResults] = useState<any>(null);
  const [perfResult, setPerfResult] = useState<number>(0);
  const [openTime, setOpenTime] = useState(0);

  useEffect(() => {
    const work = async () => {
      let start = performance.now();
      let dummyDB = open({
        name: 'dummyDb.sqlite',
      });
      setOpenTime(performance.now() - start);

      try {
        const results = await runTests();
        setServerResults(allTestsPassed(results));
        setResults(results);
      } catch (e) {
        setServerResults(false);
      }

      setTimeout(() => {
        try {
          global?.gc?.();
          let perfRes = performanceTest();
          setPerfResult(perfRes);
        } catch (e) {
          // intentionally left blank
        }
      }, 1000);
    };

    work();

    return () => {
      stopServer();
    };
  }, []);

  // const shareDb = async () => {
  // try {
  //   const db = open({
  //     name: 'shareableDb.sqlite',
  //   });
  //   await db.execute(
  //     'CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY, name TEXT)',
  //   );
  //   await db.execute("INSERT INTO test (name) VALUES ('test')");
  //   const res = await db.execute('SELECT * FROM test');
  //   console.log(res);
  //   await db.close();
  //   await Share.open({
  //     url: 'file://' + db.getDbPath(),
  //     failOnCancel: false,
  //     type: 'application/x-sqlite3',
  //   });
  // } catch (e) {
  //   console.log(e);
  // }
  // };

  return (
    <SafeAreaProvider>
      <SafeAreaView style={styles.container}>
        <View>
          <Text style={styles.performanceText}>
            Open DB time: {openTime.toFixed(0)} ms
          </Text>
          <Text style={styles.performanceText}>
            100_000 query time: {perfResult.toFixed(0)} ms
          </Text>
        </View>
        <View style={styles.results}>{displayResults(results)}</View>
      </SafeAreaView>
    </SafeAreaProvider>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#222',
    gap: 4,
    padding: 10,
  },
  results: {
    flex: 1,
  },
  performanceText: {
    color: 'white',
    fontSize: 16,
  },
});
