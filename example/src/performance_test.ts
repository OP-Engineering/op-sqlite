import {open} from '@op-engineering/op-sqlite';

const ITERATIONS = 1000;

export function performanceTest() {
  const db = open({
    name: 'perfTest.sqlite',
  });

  // Create table with 14 columns
  db.executeSync(
    `CREATE TABLE IF NOT EXISTS perf_table (
      id INTEGER PRIMARY KEY,
      col1 TEXT, col2 TEXT, col3 TEXT, col4 TEXT, col5 TEXT, col6 TEXT, col7 TEXT,
      col8 TEXT, col9 TEXT, col10 TEXT, col11 TEXT, col12 TEXT, col13 TEXT, col14 TEXT
    )`,
  );
  // Clear table
  db.executeSync('DELETE FROM perf_table');
  const testRow =Array(14).fill('test') ;

  let start = performance.now();

  for (let i = 0; i < 1_000; i++) {
    // Insert a single row for querying
    db.executeSync(
      `INSERT INTO perf_table (
        col1, col2, col3, col4, col5, col6, col7,
        col8, col9, col10, col11, col12, col13, col14
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
      testRow,
    );
  }

  for (let i = 0; i < 100000; i++) {
    db.executeSync('SELECT * FROM perf_table WHERE id = 1');
  }
  const end = performance.now();
  // console.log(`Queried 100000 times in ${end - start} ms`);

  // await db.close();
  return end - start;
}

export function insertTest() {
  const db = open({
    name: 'insertTest.sqlite'
  });


  db.executeSync("DROP TABLE IF EXISTS bench");
  db.executeSync(
    "CREATE TABLE bench (id INTEGER PRIMARY KEY, name TEXT, value REAL)",
  );

  // sync inserts
  for (let i = 0; i < ITERATIONS; i++) {
    db.executeSync("INSERT INTO bench VALUES (?,?,?)", [i, `n${i}`, i * 1.5]);
  }

  // select all
  let t = performance.now();
  for (let i = 0; i < ITERATIONS; i++) {
    db.executeSync("SELECT * FROM bench");
  }

  return performance.now() - t;
}
