import clsx from 'clsx';
import {styled} from 'nativewind';
import {useEffect, useState} from 'react';
import {SafeAreaView, ScrollView, Text, View} from 'react-native';
import {BridgeServer} from 'react-native-http-bridge-refurbished';
import 'reflect-metadata';
import {constantsTests} from './tests/constants.spec';
import {registerHooksTests} from './tests/hooks.spec';
import {blobTests, dbSetupTests, queriesTests, runTests} from './tests/index';
import {preparedStatementsTests} from './tests/preparedStatements.spec';
import {reactiveTests} from './tests/reactive.spec';

const StyledScrollView = styled(ScrollView, {
  props: {
    contentContainerStyle: true,
  },
});

export default function App() {
  const [results, setResults] = useState<any>([]);
  useEffect(() => {
    setResults([]);
    runTests(
      dbSetupTests,
      queriesTests,
      blobTests,
      registerHooksTests,
      preparedStatementsTests,
      constantsTests,
      reactiveTests,
    ).then(setResults);

    const server = new BridgeServer('http_service', true);

    server.get('/test_results', async (_req, _res) => {
      return {results};
    });

    server.listen(10424);

    return () => {
      server.stop();
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

  return (
    <SafeAreaView className="flex-1 bg-neutral-900">
      <StyledScrollView>
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
      </StyledScrollView>
    </SafeAreaView>
  );
}
