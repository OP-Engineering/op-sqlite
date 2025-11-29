import {useEffect, useState} from 'react';
import {
  // setServerError,
  setServerResults,
  stopServer,
} from './server';
import {
  displayResults,
  runTests,
  allTestsPassed,
} from '@op-engineering/op-test';
import './tests'; // import all tests to register them
import {SafeAreaProvider} from 'react-native-safe-area-context';

export default function App() {
  const [results, setResults] = useState<any>(null);
  useEffect(() => {
    // registerTests();
    runTests()
      .then(newResults => {
        setServerResults(allTestsPassed(newResults));
        setResults(newResults);
      })
      .catch(_ => {
        setServerResults(false);
      });

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

  return <SafeAreaProvider>{displayResults(results)}</SafeAreaProvider>;
}
