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
import {
  createLargeDB,
  queryLargeDB,
  querySingleRecordOnLargeDB,
} from './Database';
import {dbSetupTests, queriesTests, runTests, blobTests} from './tests/index';
import pak from '../package.json';
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

  const queryAndReload = async () => {
    queryLargeDB();
    setTimeout(() => {
      RNRestart.restart();
    }, 200);
  };

  const allTestsPassed = results.reduce((acc: boolean, r: any) => {
    return acc && r.type !== 'incorrect';
  }, true);

  const test = () => {
    const testDB = open({
      name: 'testDB',
    });

    // testDB.execute('DROP TABLE IF EXISTS segments;');

    // testDB.execute(
    //   `CREATE TABLE segments ("distance" REAL NOT NULL, "endDate" INTEGER NOT NULL, "id" TEXT PRIMARY KEY, "index" INTEGER NOT NULL, "region" TEXT NOT NULL, "speed" REAL NOT NULL, "startDate" INTEGER NOT NULL, "tripId" TEXT NOT NULL, "startLat" REAL NOT NULL, "startLng" REAL NOT NULL, "endLat" REAL NOT NULL, "endLng" REAL NOT NULL) STRICT;`,
    // );

    const sql = `SELECT EXISTS (
      SELECT 1
      FROM sqlite_master
      WHERE type='table' 
      AND name='your_table_name'
    );
    `;
    testDB.execute(sql);
  };

  return (
    <SafeAreaView className="flex-1 bg-neutral-900">
      <StyledScrollView>
        <Text className=" text-white text-2xl p-2">
          {pak.name.split('_').join(' ')}
        </Text>

        <View className="flex-row p-2 bg-neutral-800 items-center">
          <Text className={'font-bold flex-1 text-white'}>Tools</Text>
        </View>
        <Button title="Test" onPress={test} />
        <Button title="Open Sample DB" onPress={openSampleDB} />
        <Button title="Reload app middle of query" onPress={queryAndReload} />
        <Button title="Create 300k Record DB" onPress={createLargeDb} />
        <Button title="Query 300k Records" onPress={queryLargeDb} />
        <Button title="Query single record" onPress={querySingleRecord} />
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
