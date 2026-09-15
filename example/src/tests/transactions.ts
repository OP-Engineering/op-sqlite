import { type DB, isLibsql, isTurso, open, type Transaction } from "@op-engineering/op-sqlite";
import { afterEach, beforeEach, describe, expect, it } from "@op-engineering/op-test";
import { chance } from "./utils";

describe("Transactions", () => {
  let db: DB;

  beforeEach(async () => {
    db = open({
      name: "transactions.sqlite",
      encryptionKey: "test",
    });

    await db.execute("DROP TABLE IF EXISTS User;");
    await db.execute(
      "CREATE TABLE User (id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL, nickname TEXT) STRICT;",
    );
  });

  afterEach(() => {
    if (db) {
      db.delete();
      // @ts-expect-error
      db = null;
    }
  });

  it("Transaction, auto commit", async () => {
    const id = chance.integer();
    const name = chance.name();
    const age = chance.integer();
    const networth = chance.floating();

    await db.transaction(async (tx) => {
      const res = await tx.execute(
        'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
        [id, name, age, networth],
      );

      expect(res.rowsAffected).toEqual(1);
      expect(res.insertId).toEqual(1);
      // expect(res.metadata).toEqual([]);
      expect(res.rows).toDeepEqual([]);
      expect(res.rows?.length).toEqual(0);
    });

    const res = await db.execute("SELECT * FROM User");
    expect(res.rows).toDeepEqual([
      {
        id,
        name,
        age,
        networth,
        nickname: null,
      },
    ]);
  });

  it("Transaction, manual commit", async () => {
    const id = chance.integer();
    const name = chance.name();
    const age = chance.integer();
    const networth = chance.floating();

    await db.transaction(async (tx) => {
      const res = await tx.execute(
        'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
        [id, name, age, networth],
      );

      expect(res.rowsAffected).toEqual(1);
      expect(res.insertId).toEqual(1);
      expect(res.rows).toDeepEqual([]);
      expect(res.rows?.length).toEqual(0);

      tx.commit();
    });

    const res = await db.execute("SELECT * FROM User");
    expect(res.rows).toDeepEqual([
      {
        id,
        name,
        age,
        networth,
        nickname: null,
      },
    ]);
  });

  it("Transaction, executed in order", async () => {
    const xs = 10;
    const actual: unknown[] = [];

    // ARRANGE: Generate expected data
    const id = chance.integer();
    const name = chance.name();
    const age = chance.integer();

    // ACT: Start multiple transactions to upsert and select the same record
    const promises = [];
    for (let i = 1; i <= xs; i++) {
      const promised = db.transaction(async (tx) => {
        // ACT: Upsert statement to create record / increment the value
        await tx.execute(
          `
                INSERT OR REPLACE INTO [User] ([id], [name], [age], [networth])
                SELECT ?, ?, ?,
                  IFNULL((
                    SELECT [networth] + 1000
                    FROM [User]
                    WHERE [id] = ?
                  ), 0)
            `,
          [id, name, age, id],
        );

        // ACT: Select statement to get incremented value and store it for checking later
        const results = await tx.execute("SELECT [networth] FROM [User] WHERE [id] = ?", [id]);

        actual.push(results.rows[0]!.networth);
      });

      promises.push(promised);
    }

    // ACT: Wait for all transactions to complete
    await Promise.all(promises);

    // ASSERT: That the expected values where returned
    const expected = Array(xs)
      .fill(0)
      .map((_, index) => index * 1000);

    expect(actual).toDeepEqual(expected);
  });

  it("Incorrect transaction, manual rollback", async () => {
    const id = chance.string();
    const name = chance.name();
    const age = chance.integer();
    const networth = chance.floating();

    await db.transaction(async (tx) => {
      try {
        await tx.execute('INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)', [
          id,
          name,
          age,
          networth,
        ]);
      } catch (_e) {
        await tx.rollback();
      }
    });

    const res = await db.execute("SELECT * FROM User");
    expect(res.rows).toDeepEqual([]);
  });

  it("Rollback", async () => {
    const id = chance.integer();
    const name = chance.name();
    const age = chance.integer();
    const networth = chance.floating();

    await db.transaction(async (tx) => {
      await tx.execute('INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)', [
        id,
        name,
        age,
        networth,
      ]);
      await tx.rollback();
      const res = await db.execute("SELECT * FROM User");
      expect(res.rows).toDeepEqual([]);
    });
  });

  it("Transaction, rejects on callback error", async () => {
    const promised = db.transaction(() => {
      throw new Error("Error from callback");
    });

    // ASSERT: should return a promise that eventually rejects
    expect(typeof promised === "object");
    try {
      await promised;
      // expect.fail('Should not resolve');
    } catch (e) {
      // expect(e).to.be.a.instanceof(Error);
      expect((e as Error)?.message).toEqual("Error from callback");
    }
  });

  it("Transaction, rejects on invalid query", async () => {
    const promised = db.transaction(async (tx) => {
      await tx.execute("SELECT * FROM [tableThatDoesNotExist];");
    });

    // ASSERT: should return a promise that eventually rejects
    // expect(promised).to.have.property('then').that.is.a('function');
    try {
      await promised;
      // expect.fail('Should not resolve');
    } catch (e) {
      // expect(e).to.be.a.instanceof(Error);
      expect(((e as Error)?.message?.length ?? 0) > 0).toBe(true);
    }
  });

  it("Transaction, handle async callback", async () => {
    let ranCallback = false;
    const promised = db.transaction(async (tx) => {
      await new Promise<void>((done) => {
        setTimeout(() => done(), 50);
      });
      tx.execute("SELECT * FROM User;");
      ranCallback = true;
    });

    // ASSERT: should return a promise that eventually rejects
    // expect(promised).to.have.property('then').that.is.a('function');
    await promised;
    expect(ranCallback).toEqual(true);
  });

  it("Handles concurrent transactions correctly", async () => {
    const id = chance.integer();
    const name = chance.name();
    const age = chance.integer();
    const networth = chance.floating();

    const transaction1 = db.transaction(async (tx) => {
      await tx.execute('INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)', [
        id,
        name,
        age,
        networth,
      ]);
    });

    const transaction2 = db.transaction(async (tx) => {
      await tx.execute('INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)', [
        id + 1,
        name,
        age,
        networth,
      ]);
    });

    await Promise.all([transaction1, transaction2]);

    const res = await db.execute("SELECT * FROM User");
    expect(res.rows.length).toEqual(2);
  });
});

describe("Transaction finalization", () => {
  let db: DB;

  beforeEach(async () => {
    db = open({ name: "transaction-finalization.sqlite", encryptionKey: "test" });
    await db.execute("CREATE TABLE IF NOT EXISTS entries (id INTEGER PRIMARY KEY);");
  });

  afterEach(() => {
    db.delete();
  });

  it("Propagates an automatic COMMIT error, rolls back, and releases the transaction queue", async () => {
    const failure = new Error("injected COMMIT failure");
    const executeSync = db.executeSync;
    let failCommit = true;
    let rollbacks = 0;
    let actual: unknown;
    let nextCompleted = false;

    // Inject at the synchronous execution boundary to cover every backend,
    // independently of its native error reporting. All other SQL is real.
    db.executeSync = (query, params) => {
      if (query === "COMMIT;" && failCommit) {
        failCommit = false;
        throw failure;
      }
      if (query === "ROLLBACK;") rollbacks++;
      return executeSync(query, params);
    };
    try {
      const first = db
        .transaction(async (tx) => {
          await tx.execute("INSERT INTO entries VALUES (1);");
        })
        .catch((error) => {
          actual = error;
        });
      const next = db.transaction(async (tx) => {
        await tx.execute("INSERT INTO entries VALUES (2);");
        nextCompleted = true;
      });
      // Wait for both even if one fails, before restoring or deleting the DB.
      const results = await Promise.allSettled([first, next]);
      expect(actual).toBe(failure);
      expect(results[1]?.status).toEqual("fulfilled");
      expect(rollbacks).toEqual(1);
      expect(nextCompleted).toEqual(true);
      expect((await db.execute("SELECT id FROM entries ORDER BY id;")).rows).toDeepEqual([{ id: 2 }]);
    } finally {
      db.executeSync = executeSync;
    }
  });

  // Turso uses a different engine. The libSQL bridge currently drops errors
  // returned by libsql_next_row, including this deferred-FK COMMIT error.
  // Keep native error propagation separate from the JS lifecycle regression
  // above, which runs on all backends. See the CI investigation in #455.
  if (!isTurso() && !isLibsql()) {
    it("Rejects a failed automatic COMMIT, rolls back, and runs the next transaction", async () => {
      await db.execute("PRAGMA foreign_keys = ON;");
      await db.execute("CREATE TABLE parent (id INTEGER PRIMARY KEY);");
      await db.execute(
        "CREATE TABLE child (parent_id INTEGER REFERENCES parent(id) DEFERRABLE INITIALLY DEFERRED);",
      );
      let insertCompleted = false;
      let commitError: unknown;
      try {
        await db.transaction(async (tx) => {
          await tx.execute("INSERT INTO entries VALUES (1);");
          await tx.execute("INSERT INTO child VALUES (999);");
          insertCompleted = true;
        });
      } catch (error) {
        commitError = error;
      }

      expect(insertCompleted).toEqual(true);
      if (!String(commitError).toLowerCase().includes("foreign key")) {
        throw new Error(`Expected a foreign-key COMMIT error, received: ${String(commitError)}`);
      }
      expect((await db.execute("SELECT * FROM entries;")).rows).toDeepEqual([]);
      expect((await db.execute("SELECT * FROM child;")).rows).toDeepEqual([]);

      await db.transaction(async (tx) => {
        await tx.execute("INSERT INTO parent VALUES (1);");
        await tx.execute("INSERT INTO child VALUES (1);");
      });
      expect((await db.execute("SELECT * FROM child;")).rows).toDeepEqual([{ parent_id: 1 }]);
    });
  }

  for (const finalization of ["automatic commit", "explicit commit", "explicit rollback"] as const) {
    it(`Keeps the transaction handle finalized after ${finalization}`, async () => {
      let retained: Transaction | undefined;
      await db.transaction(async (tx) => {
        retained = tx;
        await tx.execute("INSERT INTO entries VALUES (1);");
        if (finalization === "explicit commit") tx.commit();
        if (finalization === "explicit rollback") tx.rollback();
      });
      if (!retained) throw new Error("Transaction callback did not run");
      const tx = retained;

      // An expired handle must not write or finalize a later transaction.
      await db.transaction(async (next) => {
        for (const operation of [
          () => tx.execute("INSERT INTO entries VALUES (99);"),
          () => tx.commit(),
          () => tx.rollback(),
        ]) {
          let rejected = false;
          try {
            await operation();
          } catch (error) {
            rejected = String(error).includes("finalized transaction");
          }
          expect(rejected).toEqual(true);
        }
        await next.execute("INSERT INTO entries VALUES (2);");
      });
      expect((await db.execute("SELECT id FROM entries ORDER BY id;")).rows).toDeepEqual(
        finalization === "explicit rollback" ? [{ id: 2 }] : [{ id: 1 }, { id: 2 }],
      );
    });
  }

  it("Preserves a callback error after explicit COMMIT without rolling back committed data", async () => {
    const failure = new Error("callback failed after COMMIT");
    let actual: unknown;
    try {
      await db.transaction(async (tx) => {
        await tx.execute("INSERT INTO entries VALUES (1);");
        tx.commit();
        throw failure;
      });
    } catch (error) {
      actual = error;
    }
    expect(actual).toBe(failure);
    expect((await db.execute("SELECT * FROM entries;")).rows).toDeepEqual([{ id: 1 }]);
    await db.transaction(async (tx) => {
      await tx.execute("INSERT INTO entries VALUES (2);");
    });
    expect((await db.execute("SELECT COUNT(*) AS count FROM entries;")).rows[0]?.count).toEqual(2);
  });
});
