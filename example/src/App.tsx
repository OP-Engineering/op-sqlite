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
        <Text className=" text-white text-xl">OP SQLite</Text>

        <Text className="text-lg text-white mt-8">Benchmarks</Text>
        <Button title="Create 300k Record DB" onPress={createLargeDb} />
        <Button title="Query 300k Records" onPress={queryLargeDb} />
        {isLoading && <ActivityIndicator color={'white'} size="large" />}
        {times.map(t => {
          return (
            <Text className="self-center text-white">{t.toFixed(0)}ms</Text>
          );
        })}

        {!!times.length && (
          <Text className="text-lg text-green-500 self-center">
            {(times.reduce((acc, t) => (acc += t), 0) / times.length).toFixed(
              0,
            )}{' '}
            ms average
          </Text>
        )}

        <Text className="text-lg text-white mt-8">Tests</Text>
        {results
          .sort((a: any, b: any) => {
            return a.type === 'incorrect' ? -1 : 1;
          })
          .map((r: any, i: number) => {
            if (r.type === 'grouping') {
              return null;
            }

            if (r.type === 'incorrect') {
              return (
                <View className="border-b border-neutral-600 py-2 flex-row">
                  <Text key={i} className="text-white flex-1">
                    {r.description}: {r.errorMsg}
                  </Text>
                  <Text>🟥</Text>
                </View>
              );
            }

            return (
              <View className="border-b border-neutral-600 py-2 flex-row">
                <Text key={i} className="text-white flex-1">
                  {r.description}
                </Text>
                <Text>🟩</Text>
              </View>
            );
          })}
      </ScrollView>
    </SafeAreaView>
  );
}
