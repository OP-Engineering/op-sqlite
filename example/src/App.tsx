import React, {useEffect, useState} from 'react';
import {
  ActivityIndicator,
  Button,
  SafeAreaView,
  ScrollView,
  Text,
} from 'react-native';
import 'reflect-metadata';
import {createLargeDB, queryLargeDB} from './Database';
import {registerBaseTests, runTests} from './tests/index';

export default function App() {
  const [isLoading, setIsLoading] = useState(false);
  const [results, setResults] = useState<any>([]);
  const [times, setTimes] = useState<number[]>([]);

  useEffect(() => {
    setResults([]);
    runTests(registerBaseTests).then(setResults);
  }, []);

  const createLargeDb = async () => {
    setIsLoading(true);
    await createLargeDB();
    setIsLoading(false);
  };

  const queryLargeDb = async () => {
    setIsLoading(true);
    const times = await queryLargeDB();
    setTimes(times);
    setIsLoading(false);
  };

  return (
    <SafeAreaView className="flex-1 bg-neutral-900">
      <ScrollView className="p-4">
        <Text className="font-bold text-white text-lg text-center">
          OP SQLite
        </Text>

        <Text className="text-xl text-white">Benchmarks</Text>
        <Button title="Create 300k Record DB" onPress={createLargeDb} />
        <Button title="Query 300k Records" onPress={queryLargeDb} />
        {isLoading && <ActivityIndicator color={'white'} size="large" />}
        {times.map(t => {
          return (
            <Text className="self-center text-white">{t.toFixed(0)}ms</Text>
          );
        })}

        {!!times.length && (
          <Text className="text-xl text-green-500 self-center">
            {(times.reduce((acc, t) => (acc += t), 0) / times.length).toFixed(
              0,
            )}{' '}
            ms average
          </Text>
        )}

        <Text className="text-xl text-white">Tests</Text>
        {results.map((r: any, i: number) => {
          if (r.type === 'grouping') {
            return (
              <Text key={i} className="mt-3 text-white">
                {r.description}
              </Text>
            );
          }

          if (r.type === 'incorrect') {
            return (
              <Text key={i} className="mt-1 text-white">
                🔴 {r.description}: {r.errorMsg}
              </Text>
            );
          }

          return (
            <Text key={i} className="mt-1 text-white">
              🟢 {r.description}
            </Text>
          );
        })}
      </ScrollView>
    </SafeAreaView>
  );
}
