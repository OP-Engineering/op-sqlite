import * as fs from "node:fs";
import * as path from "node:path";
import * as os from "node:os";
import { isIOSEmbedded, isLibsql, isSQLCipher, open } from "./index";

describe("op-sqlite Node.js tests", () => {
  let db: ReturnType<typeof open>;

  beforeAll(() => {
    db = open({ name: "test.sqlite", location: "./" });
  });

  afterAll(() => {
    db.close();

    const cleanupDb = open({ name: "test.sqlite", location: "./" });
    cleanupDb.delete();

    const cleanupDb2 = open({ name: "test2.sqlite", location: "./" });
    cleanupDb2.delete();
  });

  test("Database opens successfully", () => {
    const path = db.getDbPath();
    expect(path).toContain("test.sqlite");
  });

  test("Create table", () => {
    db.executeSync(
      "CREATE TABLE IF NOT EXISTS test_users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)",
    );
  });

  test("Insert data", () => {
    const result = db.executeSync("INSERT INTO test_users (name, age) VALUES (?, ?)", [
      "Alice",
      30,
    ]);
    expect(result.rowsAffected).toBe(1);
    expect(result.insertId).toBeDefined();
  });

  test("Query data", async () => {
    const result = await db.execute("SELECT * FROM test_users WHERE name = ?", ["Alice"]);
    expect(result.rows.length).toBe(1);
    expect(result.rows[0].name).toBe("Alice");
    expect(result.rows[0].age).toBe(30);
  });

  test("Query without parameters", () => {
    const result = db.executeSync("SELECT COUNT(*) as count FROM test_users");
    const row: number = result.rows.length;
    expect(row).toBeGreaterThanOrEqual(1);
  });

  test("Update data", () => {
    const result = db.executeSync("UPDATE test_users SET age = ? WHERE name = ?", [31, "Alice"]);
    expect(result.rowsAffected).toBe(1);
  });

  test("Verify update", () => {
    const result = db.executeSync("SELECT age FROM test_users WHERE name = ?", ["Alice"]);
    expect(result.rows[0].age).toBe(31);
  });

  test("Execute raw query", async () => {
    const result = await db.executeRaw("SELECT name, age FROM test_users WHERE name = ?", [
      "Alice",
    ]);
    expect(Array.isArray(result.rawRows)).toBe(true);
    expect(Array.isArray(result.rawRows[0])).toBe(true);
    expect(result.rawRows[0][0]).toBe("Alice");
    expect(result.rawRows[0][1]).toBe(31);
    expect(result.columnNames).toEqual(["name", "age"]);
  });

  test("Execute batch", async () => {
    const result = await db.executeBatch([
      ["INSERT INTO test_users (name, age) VALUES (?, ?)", ["Bob", 25]],
      ["INSERT INTO test_users (name, age) VALUES (?, ?)", ["Charlie", 35]],
    ]);
    expect(result.rowsAffected ?? 0).toBeGreaterThanOrEqual(2);
  });

  test("Transaction commit", async () => {
    await db.transaction(async (tx) => {
      await tx.execute("INSERT INTO test_users (name, age) VALUES (?, ?)", ["David", 40]);
      await tx.execute("INSERT INTO test_users (name, age) VALUES (?, ?)", ["Emma", 28]);
    });

    const result = db.executeSync("SELECT COUNT(*) as count FROM test_users");
    expect(result.rows[0].count).toBeGreaterThanOrEqual(5);
  });

  test("Transaction rollback", async () => {
    const beforeCount = db.executeSync("SELECT COUNT(*) as count FROM test_users").rows[0].count;

    let caught: Error | undefined;
    try {
      await db.transaction(async (tx) => {
        await tx.execute("INSERT INTO test_users (name, age) VALUES (?, ?)", ["Temporary", 99]);
        throw new Error("Rollback test");
      });
    } catch (error: any) {
      caught = error;
    }

    expect(caught).toBeDefined();
    expect(caught?.message).toBe("Rollback test");

    const afterCount = db.executeSync("SELECT COUNT(*) as count FROM test_users").rows[0].count;
    expect(beforeCount).toBe(afterCount);

    const tempRow = db.executeSync("SELECT * FROM test_users WHERE name = ?", ["Temporary"]);
    expect(tempRow.rows.length).toBe(0);
  });

  test("Prepared statement", async () => {
    const stmt = db.prepareStatement("SELECT * FROM test_users WHERE age > ?");

    stmt.bindSync([30]);
    const result1 = await stmt.execute();
    expect(result1.rows.length).toBeGreaterThanOrEqual(2);

    stmt.bindSync([25]);
    const result2 = await stmt.execute();
    expect(result2.rows.length).toBeGreaterThanOrEqual(result1.rows.length);
  });

  test("Query metadata", () => {
    const result = db.executeSync("SELECT * FROM test_users LIMIT 1");
    expect(result.metadata).toBeDefined();
    expect(result.metadata!.length).toBeGreaterThanOrEqual(3);
    expect(result.columnNames).toBeDefined();
    expect(result.columnNames).toContain("name");
  });

  test("Delete data", () => {
    const result = db.executeSync("DELETE FROM test_users WHERE name = ?", ["Bob"]);
    expect(result.rowsAffected).toBe(1);
  });

  test("Attach database", () => {
    const db2 = open({ name: "test2.sqlite", location: "./" });
    db2.executeSync("CREATE TABLE IF NOT EXISTS products (id INTEGER PRIMARY KEY, name TEXT)");
    db2.executeSync("INSERT INTO products (name) VALUES (?)", ["Laptop"]);
    db2.close();

    db.attach({
      secondaryDbFileName: "test2.sqlite",
      alias: "secondary",
      location: "./",
    });
    const result = db.executeSync("SELECT * FROM secondary.products");
    expect(result.rows.length).toBe(1);
    expect(result.rows[0].name).toBe("Laptop");
  });

  test("Detach database", () => {
    db.detach("secondary");
    expect(() => {
      db.executeSync("SELECT * FROM secondary.products");
    }).toThrow(/no such table|secondary/);
  });

  test("Load SQL file", async () => {
    const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), "opsqlite-loadfile-"));
    const sqlFilePath = path.join(tempDir, "seed.sql");
    fs.writeFileSync(
      sqlFilePath,
      [
        "CREATE TABLE loadfile_products (id INTEGER PRIMARY KEY, name TEXT)",
        "INSERT INTO loadfile_products (name) VALUES ('Widget')",
        "INSERT INTO loadfile_products (name) VALUES ('Gadget')",
      ].join(";\n") + ";\n",
    );

    try {
      const loadResult = await db.loadFile(sqlFilePath);
      expect(loadResult.commands).toBe(3);

      const result = db.executeSync("SELECT name FROM loadfile_products ORDER BY id");
      expect(result.rows.length).toBe(2);
      expect(result.rows[0].name).toBe("Widget");
      expect(result.rows[1].name).toBe("Gadget");
    } finally {
      db.executeSync("DROP TABLE IF EXISTS loadfile_products");
      fs.rmSync(tempDir, { recursive: true, force: true });
    }
  });

  test("Feature checks", () => {
    expect(isSQLCipher()).toBe(false);
    expect(isLibsql()).toBe(false);
    expect(isIOSEmbedded()).toBe(false);
  });
});
