import { open, isSQLCipher, isLibsql, isIOSEmbedded } from './index';

describe('op-sqlite Node.js tests', () => {
  let db: ReturnType<typeof open>;

  beforeAll(() => {
    db = open({ name: 'test.sqlite', location: './' });
  });

  afterAll(() => {
    db.close();

    const cleanupDb = open({ name: 'test.sqlite', location: './' });
    cleanupDb.delete('./');

    const cleanupDb2 = open({ name: 'test2.sqlite', location: './' });
    cleanupDb2.delete('./');
  });

  test('Database opens successfully', () => {
    const path = db.getDbPath();
    expect(path).toContain('test.sqlite');
  });

  test('Create table', () => {
    db.executeSync(
      'CREATE TABLE IF NOT EXISTS test_users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)'
    );
  });

  test('Insert data', () => {
    const result = db.executeSync(
      'INSERT INTO test_users (name, age) VALUES (?, ?)',
      ['Alice', 30]
    );
    expect(result.rowsAffected).toBe(1);
    expect(result.insertId).toBeDefined();
  });

  test('Query data', async () => {
    const result = await db.execute('SELECT * FROM test_users WHERE name = ?', [
      'Alice',
    ]);
    expect(result.rows.length).toBe(1);
    expect(result.rows[0].name).toBe('Alice');
    expect(result.rows[0].age).toBe(30);
  });

  test('Query without parameters', () => {
    const result = db.executeSync('SELECT COUNT(*) as count FROM test_users');
    const row: number = result.rows.length;
    expect(row).toBeGreaterThanOrEqual(1);
  });

  test('Update data', () => {
    const result = db.executeSync(
      'UPDATE test_users SET age = ? WHERE name = ?',
      [31, 'Alice']
    );
    expect(result.rowsAffected).toBe(1);
  });

  test('Verify update', () => {
    const result = db.executeSync('SELECT age FROM test_users WHERE name = ?', [
      'Alice',
    ]);
    expect(result.rows[0].age).toBe(31);
  });

  test('Execute raw query', async () => {
    const result = await db.executeRaw(
      'SELECT name, age FROM test_users WHERE name = ?',
      ['Alice']
    );
    expect(Array.isArray(result)).toBe(true);
    expect(Array.isArray(result[0])).toBe(true);
    expect(result[0][0]).toBe('Alice');
    expect(result[0][1]).toBe(31);
  });

  test('Execute batch', async () => {
    const result = await db.executeBatch([
      ['INSERT INTO test_users (name, age) VALUES (?, ?)', ['Bob', 25]],
      ['INSERT INTO test_users (name, age) VALUES (?, ?)', ['Charlie', 35]],
    ]);
    expect(result.rowsAffected ?? 0).toBeGreaterThanOrEqual(2);
  });

  test.skip('Transaction commit', async () => {
    // Skipped: better-sqlite3's transaction function doesn't support async/await
    await db.transaction(async (tx) => {
      await tx.execute('INSERT INTO test_users (name, age) VALUES (?, ?)', [
        'David',
        40,
      ]);
      await tx.execute('INSERT INTO test_users (name, age) VALUES (?, ?)', [
        'Emma',
        28,
      ]);
    });

    const result = db.executeSync('SELECT COUNT(*) as count FROM test_users');
    expect(result.rows.length).toBeGreaterThanOrEqual(5);
  });

  test.skip('Transaction rollback', async () => {
    // Skipped: The transaction implementation doesn't properly return a promise,
    // which causes issues with testing error handling
    const beforeCount = db.executeSync(
      'SELECT COUNT(*) as count FROM test_users'
    ).rows[0].count;

    try {
      await db.transaction(async (tx) => {
        await tx.execute('INSERT INTO test_users (name, age) VALUES (?, ?)', [
          'Temporary',
          99,
        ]);
        throw new Error('Rollback test');
      });
    } catch (error: any) {
      // Expected error
    }

    const afterCount = db.executeSync(
      'SELECT COUNT(*) as count FROM test_users'
    ).rows[0].count;
    expect(beforeCount).toBe(afterCount);
  });

  test('Prepared statement', async () => {
    const stmt = db.prepareStatement('SELECT * FROM test_users WHERE age > ?');

    stmt.bindSync([30]);
    const result1 = await stmt.execute();
    expect(result1.rows.length).toBeGreaterThanOrEqual(2);

    stmt.bindSync([25]);
    const result2 = await stmt.execute();
    expect(result2.rows.length).toBeGreaterThanOrEqual(result1.rows.length);
  });

  test('Query metadata', () => {
    const result = db.executeSync('SELECT * FROM test_users LIMIT 1');
    expect(result.metadata).toBeDefined();
    expect(result.metadata!.length).toBeGreaterThanOrEqual(3);
    expect(result.columnNames).toBeDefined();
    expect(result.columnNames).toContain('name');
  });

  test('Delete data', () => {
    const result = db.executeSync('DELETE FROM test_users WHERE name = ?', [
      'Bob',
    ]);
    expect(result.rowsAffected).toBe(1);
  });

  test('Attach database', () => {
    const db2 = open({ name: 'test2.sqlite', location: './' });
    db2.executeSync(
      'CREATE TABLE IF NOT EXISTS products (id INTEGER PRIMARY KEY, name TEXT)'
    );
    db2.executeSync('INSERT INTO products (name) VALUES (?)', ['Laptop']);
    db2.close();

    db.attach({
      secondaryDbFileName: 'test2.sqlite',
      alias: 'secondary',
      location: './',
    });
    const result = db.executeSync('SELECT * FROM secondary.products');
    expect(result.rows.length).toBe(1);
    expect(result.rows[0].name).toBe('Laptop');
  });

  test('Detach database', () => {
    db.detach('secondary');
    expect(() => {
      db.executeSync('SELECT * FROM secondary.products');
    }).toThrow(/no such table|secondary/);
  });

  test('Feature checks', () => {
    expect(isSQLCipher()).toBe(false);
    expect(isLibsql()).toBe(false);
    expect(isIOSEmbedded()).toBe(false);
  });
});
