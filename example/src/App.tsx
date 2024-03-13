import React, {useEffect, useState} from 'react';
import {
  ActivityIndicator,
  Button,
  Clipboard,
  SafeAreaView,
  ScrollView,
  Text,
  View,
} from 'react-native';
import 'reflect-metadata';
import {
  createLargeDB,
  queryLargeDB,
  querySingleRecordOnLargeDB,
} from './Database';
import {dbSetupTests, queriesTests, runTests, blobTests} from './tests/index';
import {styled} from 'nativewind';
import RNRestart from 'react-native-restart';
import {registerHooksTests} from './tests/hooks.spec';
import {open} from '@op-engineering/op-sqlite';
import clsx from 'clsx';
import {preparedStatementsTests} from './tests/preparedStatements.spec';
import {constantsTests} from './tests/constants.spec';
import performance from 'react-native-performance';

const StyledScrollView = styled(ScrollView, {
  props: {
    contentContainerStyle: true,
  },
});

export default function App() {
  const [isLoading, setIsLoading] = useState(false);
  const [results, setResults] = useState<any>([]);
  const [times, setTimes] = useState<number[]>([]);
  const [accessingTimes, setAccessingTimes] = useState<number[]>([]);
  const [prepareTimes, setPrepareTimes] = useState<number[]>([]);
  const [prepareExecutionTimes, setPrepareExecutionTimes] = useState<number[]>(
    [],
  );
  const [singleRecordTime, setSingleRecordTime] = useState<number>(0);
  const [rawExecutionTimes, setRawExecutionTimes] = useState<number[]>([]);
  useEffect(() => {
    setResults([]);
    runTests(
      dbSetupTests,
      queriesTests,
      blobTests,
      registerHooksTests,
      preparedStatementsTests,
      constantsTests,
    ).then(setResults);
  }, []);

  const querySingleRecord = async () => {
    let start = performance.now();
    await querySingleRecordOnLargeDB();
    let end = performance.now();

    setSingleRecordTime(end - start);
  };

  const createLargeDb = async () => {
    setIsLoading(true);
    await createLargeDB();
    setIsLoading(false);
  };

  const queryLargeDb = async () => {
    try {
      setIsLoading(true);
      const times = await queryLargeDB();
      setTimes(times.loadFromDb);
      setAccessingTimes(times.access);
      setPrepareTimes(times.prepare);
      setPrepareExecutionTimes(times.preparedExecution);
      setRawExecutionTimes(times.rawExecution);
    } catch (e) {
      console.error(e);
    } finally {
      setIsLoading(false);
    }
  };

  const copyDbPathToClipboad = async () => {
    const db = await open({name: 'dbPath.sqlite'});
    db.execute('CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT)');
    const path = db.getDbPath();
    await db.close();
    console.warn(path);
    Clipboard.setString(path);
  };

  const allTestsPassed = results.reduce((acc: boolean, r: any) => {
    return acc && r.type !== 'incorrect';
  }, true);

  return (
    <SafeAreaView className="flex-1 bg-neutral-900">
      <StyledScrollView>
        <View className="flex-row p-2 bg-neutral-800 items-center">
          <Text className={'font-bold flex-1 text-white'}>Tools</Text>
        </View>
        <Button
          title="Copy DB Path to clipboard"
          onPress={copyDbPathToClipboad}
        />
        <Button title="Create 300k Record DB" onPress={createLargeDb} />
        <Button title="Query 300k Records" onPress={queryLargeDb} />
        {isLoading && <ActivityIndicator color={'white'} size="large" />}

        {!!singleRecordTime && (
          <Text className="text-lg text-white self-center">
            Query single record time: {singleRecordTime.toFixed(2)}ms
          </Text>
        )}

        {!!times.length && (
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

        {/* <UpdateHookPage /> */}

        <Text
          className={clsx('font-bold flex-1 text-white p-2 mt-4', {
            'bg-green-500': allTestsPassed,
            'bg-red-500': !allTestsPassed,
          })}>
          Test Suite
        </Text>
        {results.map((r: any, i: number) => {
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
            <View className="border-b border-neutral-800 p-2 flex-row" key={i}>
              <Text className="text-green-500 flex-1">{r.description}</Text>
            </View>
          );
        })}
      </StyledScrollView>
    </SafeAreaView>
  );
}
