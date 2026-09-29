import { type DB, isTurso, open } from "@op-engineering/op-sqlite";
import { afterEach, beforeEach, describe, expect, it } from "@op-engineering/op-test";

async function captureError(fn: () => unknown): Promise<Error> {
  try {
    await fn();
  } catch (e) {
    return e as Error;
  }

  throw new Error("Expected the call to fail, it did not");
}

// A trigger doing RAISE(ROLLBACK) makes SQLite roll the transaction back on its
// own, so the wrapper's ROLLBACK must not replace the "vetoed" error with
// "cannot rollback - no transaction is active".
describe("Rollback after SQLite already rolled back", () => {
  let db: DB;

  beforeEach(async () => {
    db = open({
      name: "rollback.sqlite",
      encryptionKey: "test",
    });

    await db.execute("DROP TABLE IF EXISTS t;");
    await db.execute("CREATE TABLE t (x INTEGER);");
    // Turso's engine does not support triggers
    if (!isTurso()) {
      await db.execute(
        "CREATE TRIGGER veto BEFORE INSERT ON t WHEN NEW.x = 13 BEGIN SELECT RAISE(ROLLBACK, 'vetoed'); END;",
      );
    }
  });

  afterEach(() => {
    if (db) {
      db.delete();
      // @ts-expect-error
      db = null;
    }
  });

  it("executeBatch rejects with the original error", async () => {
    if (isTurso()) {
      return;
    }

    const error = await captureError(() =>
      db.executeBatch([
        ["INSERT INTO t VALUES (?)", [1]],
        ["INSERT INTO t VALUES (?)", [13]],
      ]),
    );

    expect(error.message).toContain("vetoed");

    const res = await db.execute("SELECT COUNT(*) AS count FROM t");
    expect(res.rows[0]!.count).toEqual(0);
  });

  it("transaction rejects with the original error", async () => {
    if (isTurso()) {
      return;
    }

    const error = await captureError(() =>
      db.transaction(async (tx) => {
        await tx.execute("INSERT INTO t VALUES (?)", [1]);
        await tx.execute("INSERT INTO t VALUES (?)", [13]);
      }),
    );

    expect(error.message).toContain("vetoed");

    const res = await db.execute("SELECT COUNT(*) AS count FROM t");
    expect(res.rows[0]!.count).toEqual(0);
  });

  it("tx.rollback() after SQLite rolled back does not throw", async () => {
    if (isTurso()) {
      return;
    }

    await db.transaction(async (tx) => {
      try {
        await tx.execute("INSERT INTO t VALUES (?)", [13]);
      } catch {
        tx.rollback();
      }
    });

    const res = await db.execute("SELECT COUNT(*) AS count FROM t");
    expect(res.rows[0]!.count).toEqual(0);
  });

  it("the connection is usable after the veto", async () => {
    if (isTurso()) {
      return;
    }

    await captureError(() => db.executeBatch([["INSERT INTO t VALUES (?)", [13]]]));

    await db.executeBatch([["INSERT INTO t VALUES (?)", [1]]]);

    const res = await db.execute("SELECT COUNT(*) AS count FROM t");
    expect(res.rows[0]!.count).toEqual(1);
  });

  it("a failed statement still rolls back the batch", async () => {
    const error = await captureError(() =>
      db.executeBatch([
        ["INSERT INTO t VALUES (?)", [1]],
        ["INSERT INTO tableThatDoesNotExist VALUES (?)", [2]],
      ]),
    );

    expect(error.message).toContain("no such table");

    const res = await db.execute("SELECT COUNT(*) AS count FROM t");
    expect(res.rows[0]!.count).toEqual(0);
  });
});
