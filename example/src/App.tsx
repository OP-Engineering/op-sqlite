import clsx from 'clsx';
import {useEffect, useState} from 'react';
import {
  Button,
  Clipboard,
  SafeAreaView,
  ScrollView,
  Text,
  View,
} from 'react-native';
import 'reflect-metadata';
import {constantsTests} from './tests/constants.spec';
import {registerHooksTests} from './tests/hooks.spec';
import {blobTests, dbSetupTests, queriesTests, runTests} from './tests/index';
import {preparedStatementsTests} from './tests/preparedStatements.spec';
import {reactiveTests} from './tests/reactive.spec';
import {setServerResults, startServer, stopServer} from './server';
import {open} from '@op-engineering/op-sqlite';
import Share from 'react-native-share';

export default function App() {
  const [results, setResults] = useState<any>([]);
  useEffect(() => {
    runTests(
      dbSetupTests,
      queriesTests,
      blobTests,
      registerHooksTests,
      preparedStatementsTests,
      constantsTests,
      reactiveTests,
    ).then(results => {
      setServerResults(results as any);
      setResults(results);
    });

    startServer();

    return () => {
      stopServer();
    };
  }, []);

  let passingTests = 0;
  let failingTests = 0;

  const allTestsPassed = results.reduce((acc: boolean, r: any) => {
    if (r.type === 'incorrect') {
      failingTests++;
    } else if (r.type === 'correct') {
      passingTests++;
    }

    return acc && r.type !== 'incorrect';
  }, true);

  const shareDb = async () => {
    try {
      const db = open({
        name: 'shareableDb.sqlite',
      });

      await db.execute(
        'CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY, name TEXT)',
      );
      await db.execute("INSERT INTO test (name) VALUES ('test')");
      const res = await db.execute('SELECT * FROM test');
      console.log(res);

      await db.close();
      await Share.open({
        url: 'file://' + db.getDbPath(),
        failOnCancel: false,
        type: 'application/x-sqlite3',
      });
    } catch (e) {
      console.log(e);
    }
  };

  const copyDbPathToClipboad = async () => {
    const db = await open({name: 'shareableDb.sqlite'});
    const path = db.getDbPath();
    await db.close();
    console.warn(path);
    Clipboard.setString(path);
  };

  return (
    <SafeAreaView className="flex-1 bg-neutral-900">
      <ScrollView>
        <Button title="Share DB" onPress={shareDb} />
        <Button title="Copy DB Path" onPress={copyDbPathToClipboad} />
        <Text
          className={clsx('font-bold flex-1 text-white p-2 mt-4', {
            'bg-green-500': allTestsPassed,
            'bg-red-500': !allTestsPassed,
          })}>
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
                  key={i}>
                  <Text className="text-red-500 flex-1">
                    {r.description}: {r.errorMsg}
                  </Text>
                </View>
              );
            }

            return (
              <View
                className="border-b border-neutral-800 p-2 flex-row"
                key={i}>
                <Text className="text-green-500 flex-1">{r.description}</Text>
              </View>
            );
          })}
      </ScrollView>
    </SafeAreaView>
  );
}
