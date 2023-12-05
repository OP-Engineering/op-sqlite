import React, {useEffect, useState} from 'react';
import {
  ActivityIndicator,
  Button,
  SafeAreaView,
  ScrollView,
  Text,
  View,
} from 'react-native';
import 'reflect-metadata';
import {createLargeDB, queryLargeDB} from './Database';
import {dbSetupTests, queriesTests, runTests, blobTests} from './tests/index';
import pak from '../package.json';
import {styled} from 'nativewind';
import RNRestart from 'react-native-restart';
import {registerHooksTests} from './tests/hooks.spec';
import {open} from '@op-engineering/op-sqlite';
import clsx from 'clsx';
import {preparedStatementsTests} from './tests/preparedStatements.spect';
import performance from 'react-native-performance';
import {MMKV} from 'react-native-mmkv';

export const mmkv = new MMKV();

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
  const [sqliteMMSetTime, setSqliteMMSetTime] = useState(0);
  const [mmkvSetTime, setMMKVSetTime] = useState(0);
  const [sqliteGetTime, setSqliteMMGetTime] = useState(0);
  const [mmkvGetTime, setMMKVGetTime] = useState(0);

  useEffect(() => {
    setResults([]);
    runTests(
      dbSetupTests,
      queriesTests,
      blobTests,
      registerHooksTests,
      preparedStatementsTests,
    ).then(setResults);
  }, []);

  const createLargeDb = async () => {
    setIsLoading(true);
    await createLargeDB();
    setIsLoading(false);
  };

  const openSampleDB = async () => {
    const sampleDb = open({
      name: 'sampleDB',
    });
  };

  const queryLargeDb = async () => {
    try {
      setIsLoading(true);
      const times = await queryLargeDB();
      setTimes(times.loadFromDb);
      setAccessingTimes(times.access);
      setPrepareTimes(times.prepare);
      setPrepareExecutionTimes(times.preparedExecution);
    } catch (e) {
      console.error(e);
    } finally {
      setIsLoading(false);
    }
  };

  const testAgainstMMKV = () => {
    const db = open({
      name: 'mmkvTestDb',
    });

    db.execute('PRAGMA mmap_size=268435456');
    db.execute('PRAGMA journal_mode = OFF;');
    db.execute('DROP TABLE IF EXISTS mmkvTest;');
    db.execute('CREATE TABLE mmkvTest (text TEXT);');

    let insertStatment = db.prepareStatement(
      'INSERT INTO "mmkvTest" (text) VALUES (?)',
    );
    insertStatment.bind(['quack']);

    let start = performance.now();
    insertStatment.execute();
    let end = performance.now();
    setSqliteMMSetTime(end - start);

    start = performance.now();
    mmkv.set('mmkvDef', 'quack');
    end = performance.now();
    setMMKVSetTime(end - start);

    let readStatement = db.prepareStatement('SELECT text from mmkvTest;');
    start = performance.now();
    readStatement.execute();
    end = performance.now();
    setSqliteMMGetTime(end - start);

    start = performance.now();
    mmkv.getString('mmkvDef');
    end = performance.now();
    setMMKVGetTime(end - start);

    db.close();
  };

  const queryAndReload = async () => {
    queryLargeDB();
    setTimeout(() => {
      RNRestart.restart();
    }, 200);
  };

  const allTestsPassed = results.reduce((acc: boolean, r: any) => {
    return acc && r.type !== 'incorrect';
  }, true);

  return (
    <SafeAreaView className="flex-1 bg-neutral-900">
      <StyledScrollView>
        <Text className=" text-white text-2xl p-2">
          {pak.name.split('_').join(' ')}
        </Text>

        <View className="flex-row p-2 bg-neutral-800 items-center">
          <Text className={'font-bold flex-1 text-white'}>Tools</Text>
        </View>
        <Button title="Test against MMKV" onPress={testAgainstMMKV} />
        <View className="p-4 gap-2 items-center">
          {!!sqliteMMSetTime && (
            <Text className="text-white">
              MM SQLite Write: {sqliteMMSetTime.toFixed(2)} ms
            </Text>
          )}
          {!!mmkvSetTime && (
            <Text className="text-white">
              MMKV Write: {mmkvSetTime.toFixed(2)} ms
            </Text>
          )}
          {!!sqliteGetTime && (
            <Text className="text-white">
              MM SQLite Get: {sqliteGetTime.toFixed(2)} ms
            </Text>
          )}
          {!!mmkvGetTime && (
            <Text className="text-white">
              MMKV Get: {mmkvGetTime.toFixed(2)} ms
            </Text>
          )}
        </View>
        <Button title="Open Sample DB" onPress={openSampleDB} />
        <Button title="Reload app middle of query" onPress={queryAndReload} />
        <Button title="Create 300k Record DB" onPress={createLargeDb} />
        <Button title="Query 300k Records" onPress={queryLargeDb} />
        {isLoading && <ActivityIndicator color={'white'} size="large" />}

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
              <Text className="p-2 font-semibold text-white">
                {r.description}
              </Text>
            );
          }

          if (r.type === 'incorrect') {
            return (
              <View className="border-b border-neutral-800 p-2 flex-row">
                <Text key={i} className="text-red-500 flex-1">
                  {r.description}: {r.errorMsg}
                </Text>
              </View>
            );
          }

          return (
            <View className="border-b border-neutral-800 p-2 flex-row">
              <Text key={i} className="text-green-500 flex-1">
                {r.description}
              </Text>
            </View>
          );
        })}
      </StyledScrollView>
    </SafeAreaView>
  );
}
