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
import {registerHooksTests} from './tests/hooks';

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

  useEffect(() => {
    setResults([]);
    runTests(dbSetupTests, queriesTests, blobTests, registerHooksTests).then(
      setResults,
    );
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
        <Text className=" text-white text-2xl p-2">{pak.name}</Text>

        <View className="flex-row p-2 bg-neutral-600 items-center">
          <Text className={'text-lg flex-1  text-white'}>BENCHMARKS</Text>
        </View>
        <Button title="Reload app middle of query" onPress={queryAndReload} />
        <Button title="Create 300k Record DB" onPress={createLargeDb} />
        <Button title="Query 300k Records" onPress={queryLargeDb} />
        {isLoading && <ActivityIndicator color={'white'} size="large" />}

        {!!times.length && (
          <Text className="text-lg text-white self-center">
            Load from db{' '}
            {(times.reduce((acc, t) => (acc += t), 0) / times.length).toFixed(
              0,
            )}{' '}
            ms average
          </Text>
        )}
        {!!accessingTimes.length && (
          <Text className="text-lg text-white self-center">
            Read property{' '}
            {(
              accessingTimes.reduce((acc, t) => (acc += t), 0) /
              accessingTimes.length
            ).toFixed(0)}{' '}
            ms average
          </Text>
        )}

        <View className="flex-row p-2 mt-4 bg-neutral-600 items-center">
          <Text className={'text-lg flex-1  text-white'}>ALL TESTS</Text>
          {allTestsPassed ? <Text>🟩</Text> : <Text>🟥</Text>}
        </View>
        {results.map((r: any, i: number) => {
          if (r.type === 'grouping') {
            return (
              <Text className="bg-neutral-700 p-2 text-white">
                {r.description}
              </Text>
            );
          }

          if (r.type === 'incorrect') {
            return (
              <View className="border-b border-neutral-600 p-2 flex-row">
                <Text key={i} className="text-white flex-1">
                  {r.description}: {r.errorMsg}
                </Text>
                <Text>🔻</Text>
              </View>
            );
          }

          return (
            <View className="border-b border-neutral-600 p-2 flex-row">
              <Text key={i} className="text-white flex-1">
                {r.description}
              </Text>
              <Text>🟢</Text>
            </View>
          );
        })}
      </StyledScrollView>
    </SafeAreaView>
  );
}
