import {
  type DB,
  isLibsql,
  isTurso,
  open,
  type SQLBatchTuple,
  type SQLiteError,
} from "@op-engineering/op-sqlite";
import { afterEach, beforeEach, describe, expect, it } from "@op-engineering/op-test";

// https://sqlite.org/rescode.html
const SQLITE_ERROR = 1;
const SQLITE_CONSTRAINT = 19;
const SQLITE_CONSTRAINT_NOTNULL = 1299;
const SQLITE_CONSTRAINT_PRIMARYKEY = 1555;

// libsql and turso only hand back a message, there are no result codes to
// report on those backends.
const backendReportsCodes = !isLibsql() && !isTurso();

async function captureError(fn: () => unknown): Promise<SQLiteError> {
  try {
    await fn();
  } catch (e) {
    return e as SQLiteError;
  }

  throw new Error("Expected the call to fail, it did not");
}

describe("Error codes", () => {
  let db: DB;

  beforeEach(async () => {
    db = open({
      name: "errorCodes.sqlite",
      encryptionKey: "test",
    });

    await db.execute("DROP TABLE IF EXISTS User;");
    await db.execute("CREATE TABLE User (id INT PRIMARY KEY, name TEXT NOT NULL) STRICT;");
    await db.execute('INSERT INTO "User" (id, name) VALUES(?, ?)', [1, "Oscar"]);
  });

  afterEach(() => {
    if (db) {
      db.delete();
      // @ts-expect-error
      db = null;
    }
  });

  it("execute rejects with the result codes", async () => {
    if (!backendReportsCodes) {
      return;
    }

    const error = await captureError(() => db.execute("SELECT * FROM tableThatDoesNotExist"));

    expect(error.code).toEqual(SQLITE_ERROR);
    expect(error.extendedCode).toEqual(SQLITE_ERROR);
    expect(error.message).toContain("no such table");
  });

  it("execute reports the extended code of a constraint violation", async () => {
    if (!backendReportsCodes) {
      return;
    }

    const notNull = await captureError(() =>
      db.execute('INSERT INTO "User" (id, name) VALUES(?, ?)', [2, null]),
    );

    expect(notNull.code).toEqual(SQLITE_CONSTRAINT);
    expect(notNull.extendedCode).toEqual(SQLITE_CONSTRAINT_NOTNULL);

    // Same primary code, different extended one: this is the distinction that
    // cannot be made from the message alone.
    const primaryKey = await captureError(() =>
      db.execute('INSERT INTO "User" (id, name) VALUES(?, ?)', [1, "Oscar"]),
    );

    expect(primaryKey.code).toEqual(SQLITE_CONSTRAINT);
    expect(primaryKey.extendedCode).toEqual(SQLITE_CONSTRAINT_PRIMARYKEY);
  });

  it("executeSync throws with the result codes", async () => {
    if (!backendReportsCodes) {
      return;
    }

    const error = await captureError(() => db.executeSync("SELECT * FROM tableThatDoesNotExist"));

    expect(error.code).toEqual(SQLITE_ERROR);
    expect(error.extendedCode).toEqual(SQLITE_ERROR);
  });

  it("executeRaw and executeRawSync report the result codes", async () => {
    if (!backendReportsCodes) {
      return;
    }

    const asyncError = await captureError(() =>
      db.executeRaw("SELECT * FROM tableThatDoesNotExist"),
    );
    expect(asyncError.code).toEqual(SQLITE_ERROR);

    const syncError = await captureError(() =>
      db.executeRawSync("SELECT * FROM tableThatDoesNotExist"),
    );
    expect(syncError.code).toEqual(SQLITE_ERROR);
  });

  it("executeBatch rejects with the result codes", async () => {
    if (!backendReportsCodes) {
      return;
    }

    const commands: SQLBatchTuple[] = [
      ['INSERT INTO "User" (id, name) VALUES(?, ?)', [2, "Pablo"]],
      ['INSERT INTO "User" (id, name) VALUES(?, ?)', [1, "Carlos"]],
    ];

    const error = await captureError(() => db.executeBatch(commands));

    expect(error.code).toEqual(SQLITE_CONSTRAINT);
    expect(error.extendedCode).toEqual(SQLITE_CONSTRAINT_PRIMARYKEY);

    const res = await db.execute("SELECT * FROM User");
    expect(res.rows.length).toEqual(1);
  });

  it("prepared statements report the result codes", async () => {
    if (!backendReportsCodes) {
      return;
    }

    const prepareError = await captureError(() => db.prepareStatement("NOT VALID SQL"));
    expect(prepareError.code).toEqual(SQLITE_ERROR);

    const statement = db.prepareStatement('INSERT INTO "User" (id, name) VALUES(?, ?)');
    statement.bindSync([1, "Oscar"]);

    const asyncError = await captureError(() => statement.execute());
    expect(asyncError.code).toEqual(SQLITE_CONSTRAINT);
    expect(asyncError.extendedCode).toEqual(SQLITE_CONSTRAINT_PRIMARYKEY);

    const syncError = await captureError(() => statement.executeSync());
    expect(syncError.code).toEqual(SQLITE_CONSTRAINT);
    expect(syncError.extendedCode).toEqual(SQLITE_CONSTRAINT_PRIMARYKEY);
  });

  it("errors that do not come from sqlite carry no result codes", async () => {
    const error = await captureError(() => {
      db.close();
      return db.execute("SELECT 1");
    });

    expect(error.code).toEqual(undefined);
    expect(error.extendedCode).toEqual(undefined);

    // Reopen so afterEach can delete the file.
    db = open({
      name: "errorCodes.sqlite",
      encryptionKey: "test",
    });
  });
});
