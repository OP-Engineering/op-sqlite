---
sidebar_position: 10
---

# ORMs & Libs

OP-SQLite is not an ORM. It doesn't keep track of entities or creates SQL queries for you. It's pretty much a raw bindings to the sqlite3 C api. That being said, ORMs are useful (they make the easy things easier while making the hard things impossible) and there are ORMs that use op-sqlite as their main driver

## DrizzleORM

Drizzle works with op-sqlite. Follow their documentation to set up a new project:

https://orm.drizzle.team/docs/connect-op-sqlite

## TypeORM

TypeORM is not directly supported as in the past is was broken and people started opening issues on the repo. Here is an example driver you can adjust and pass to the driver param when creating a new TypeORM instance:

```ts
import { QueryResult, Transaction, open } from '@op-engineering/op-sqlite';

const enhanceQueryResult = (result: QueryResult): void => {
  result.rows.item = (idx: number) => result.rows[idx];
};

export const typeORMDriver = {
  openDatabase: (
    options: {
      name: string;
      location?: string;
      encryptionKey: string;
    },
    ok: (db: any) => void,
    fail: (msg: string) => void
  ): any => {
    try {
      if (!options.encryptionKey || options.encryptionKey.length === 0) {
        throw new Error('[op-sqlite]: Encryption key is required');
      }

      const database = open({
        location: options.location,
        name: options.name,
        encryptionKey: options.encryptionKey,
      });

      const connection = {
        executeSql: async (
          sql: string,
          params: any[] | undefined,
          ok: (res: QueryResult) => void,
          fail: (msg: string) => void
        ) => {
          try {
            const response = await database.execute(sql, params);
            enhanceQueryResult(response);
            ok(response);
          } catch (e) {
            fail(`[op-sqlite]: Error executing SQL: ${e as string}`);
          }
        },
        transaction: (
          fn: (tx: Transaction) => Promise<void>
        ): Promise<void> => {
          return database.transaction(fn);
        },
        close: (ok: any, fail: any) => {
          try {
            database.close();
            ok();
          } catch (e) {
            fail(`[op-sqlite]: Error closing db: ${e as string}`);
          }
        },
        attach: (
          dbNameToAttach: string,
          alias: string,
          location: string | undefined,
          callback: () => void
        ) => {
          database.attach(options.name, dbNameToAttach, alias, location);

          callback();
        },
        detach: (alias: string, callback: () => void) => {
          database.detach(options.name, alias);

          callback();
        },
      };

      ok(connection);

      return connection;
    } catch (e) {
      fail(`[op-sqlite]: Error opening database: ${e as string}`);
    }
  },
};
```

## PowerSync

PowerSync uses op-sqlite internally to power their synchronization engine.
