import React, {
  useEffect,
  useState,
  createContext,
  useContext,
  useRef,
} from 'react';
import {ActivityIndicator, Text, View} from 'react-native';
import {OPSQLiteConnection, open} from '@op-engineering/op-sqlcipher';

interface IDbContext {
  db: OPSQLiteConnection;
}

const DbContext = createContext<IDbContext | null>(null);

const UpdateHookPage = () => {
  const [loading, setLoading] = useState(true);
  const dbRef = useRef<OPSQLiteConnection>();

  useEffect(() => {
    function setup() {
      const db = open({name: 'updatehook_test.db'});
      db.updateHook(({table, operation}) => {
        console.log('updateHook', table, operation);
      });

      db.execute('DROP TABLE IF EXISTS updatehook_example');
      db.execute(
        'CREATE TABLE updatehook_example (id INTEGER PRIMARY KEY, description TEXT, qty INTEGER, price REAL)',
      );
      db.execute(
        'INSERT INTO updatehook_example (description, qty, price) VALUES (?, ?, ?)',
        ['Pretzels', 1, 5.5],
      );
      db.execute(
        'INSERT INTO updatehook_example (description, qty, price) VALUES (?, ?, ?)',
        ['Chips', 1, 2.25],
      );

      dbRef.current = db;
      setLoading(false);
    }
    function teardown() {
      dbRef.current?.close();
      dbRef.current = undefined;
      setLoading(true);
    }

    setup();

    return () => {
      teardown();
    };
  }, []);

  if (!dbRef.current || loading) {
    return <ActivityIndicator />;
  }

  return (
    <DbContext.Provider
      value={{
        db: dbRef.current!,
      }}>
      <UpdateHookExample />
    </DbContext.Provider>
  );
};

export default UpdateHookPage;

export function useDbContext() {
  const context = useContext(DbContext);
  if (context == null) {
    throw new Error('DbContext.Provider is missing from the component tree');
  }
  return context;
}

function UpdateHookExample() {
  const {db} = useDbContext();

  const rows =
    db.execute('SELECT * FROM updatehook_example').rows?._array ?? [];

  return (
    <View>
      <Text>Update Hook Crash</Text>
      <Text>"Reload" the app to cause it to crash.</Text>
      <View>
        {rows.map((row, i) => (
          <Text key={i}>
            {row.description} {row.qty} {row.price}
          </Text>
        ))}
      </View>
    </View>
  );
}
