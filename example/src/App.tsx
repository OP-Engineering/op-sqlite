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
import {registerTests, runTests} from './tests/index';
import pak from '../package.json';
import clsx from 'clsx';

export default function App() {
  const [isLoading, setIsLoading] = useState(false);
  const [results, setResults] = useState<any>([]);
  const [times, setTimes] = useState<number[]>([]);
  const [accessingTimes, setAccessingTimes] = useState<number[]>([]);

  useEffect(() => {
    setResults([]);
    runTests(registerTests).then(setResults);
  }, []);

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
    } catch (e) {
      console.error(e);
    } finally {
      setIsLoading(false);
    }
  };

  const allTestsPassed = results.reduce((acc: boolean, r: any) => {
    return acc && r.type !== 'incorrect';
  }, true);

  return (
    <SafeAreaView className="flex-1 bg-neutral-900">
      <View className="p-4">
        <ScrollView>
          <Text className=" text-white text-xl">{pak.name}</Text>

          <Text className="text-lg text-white mt-8">Benchmarks</Text>
          <Button title="Create 300k Record DB" onPress={createLargeDb} />
          <Button title="Query 300k Records" onPress={queryLargeDb} />
          {isLoading && <ActivityIndicator color={'white'} size="large" />}

          {!!times.length && (
            <Text className="text-lg text-green-500 self-center">
              Load from db{' '}
              {(times.reduce((acc, t) => (acc += t), 0) / times.length).toFixed(
                0,
              )}{' '}
              ms average
            </Text>
          )}
          {!!accessingTimes.length && (
            <Text className="text-lg text-green-500 self-center">
              Read property{' '}
              {(
                accessingTimes.reduce((acc, t) => (acc += t), 0) /
                accessingTimes.length
              ).toFixed(0)}{' '}
              ms average
            </Text>
          )}

          <View className="flex-row mt-8 mb-3">
            <Text
              className={clsx('text-lg flex-1', {
                'text-green-500': allTestsPassed,
                'text-red-500': !allTestsPassed,
              })}>
              Tests
            </Text>
            {allTestsPassed ? <Text>🟩</Text> : <Text>🟥</Text>}
          </View>
          {results.map((r: any, i: number) => {
            if (r.type === 'grouping') {
              return null;
            }

            if (r.type === 'incorrect') {
              return (
                <View className="border-b border-neutral-600 py-2 flex-row">
                  <Text key={i} className="text-white flex-1">
                    {r.description}: {r.errorMsg}
                  </Text>
                  <Text>🔻</Text>
                </View>
              );
            }

            return (
              <View className="border-b border-neutral-600 py-2 flex-row">
                <Text key={i} className="text-white flex-1">
                  {r.description}
                </Text>
                <Text>🟢</Text>
              </View>
            );
          })}
        </ScrollView>
      </View>
    </SafeAreaView>
  );
}
