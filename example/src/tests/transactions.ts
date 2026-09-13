import { type DB, isTurso, open, type Transaction } from "@op-engineering/op-sqlite";
import { afterEach, beforeEach, describe, expect, it } from "@op-engineering/op-test";

describe("Transaction finalization", () => {
  let db: DB;

  beforeEach(async () => {
    db = open({ name: "transaction-finalization.sqlite", encryptionKey: "test" });
    await db.execute("CREATE TABLE IF NOT EXISTS entries (id INTEGER PRIMARY KEY);");
  });

  afterEach(() => {
    db.delete();
  });

  // Turso uses a different engine; this regression requires SQLite deferred FKs.
  if (!isTurso()) {
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
      expect(String(commitError).toLowerCase().includes("foreign key")).toEqual(true);
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
        if (finalization === "explicit commit") await tx.commit();
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
        await tx.commit();
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
