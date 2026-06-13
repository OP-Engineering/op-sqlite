import { open } from "@op-engineering/op-sqlite";

export async function performanceTest() {
  const db = open({
    name: "perfTest.sqlite",
  });

  db.executeSync(
    `CREATE TABLE IF NOT EXISTS perf_table (
      id INTEGER PRIMARY KEY,
      col1 TEXT, col2 TEXT, col3 TEXT, col4 TEXT, col5 TEXT, col6 TEXT, col7 TEXT,
      col8 TEXT, col9 TEXT, col10 TEXT, col11 TEXT, col12 TEXT, col13 TEXT, col14 TEXT
    )`,
  );

  db.executeSync("DELETE FROM perf_table");
  const testRow = Array(14).fill("test");

  console.log("Starting performance test");
  let start = performance.now();

  for (let i = 0; i < 200; i++) {
    let firstPromise = db.transaction(async (tx) => {
      await tx.execute(
        `INSERT INTO perf_table (
        col1, col2, col3, col4, col5, col6, col7,
        col8, col9, col10, col11, col12, col13, col14
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
        testRow,
      );

      await tx.execute(
        `INSERT INTO perf_table (
        col1, col2, col3, col4, col5, col6, col7,
        col8, col9, col10, col11, col12, col13, col14
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
        testRow,
      );
    });

    let secondPromise = db.transaction(async (tx) => {
      await tx.execute(
        `INSERT INTO perf_table (
        col1, col2, col3, col4, col5, col6, col7,
        col8, col9, col10, col11, col12, col13, col14
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
        testRow,
      );

      await tx.execute(
        `INSERT INTO perf_table (
        col1, col2, col3, col4, col5, col6, col7,
        col8, col9, col10, col11, col12, col13, col14
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
        testRow,
      );
    });

    let thirdPromise = db.transaction(async (tx) => {
      await tx.execute(
        `INSERT INTO perf_table (
        col1, col2, col3, col4, col5, col6, col7,
        col8, col9, col10, col11, col12, col13, col14
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
        testRow,
      );

      await tx.execute(
        `INSERT INTO perf_table (
        col1, col2, col3, col4, col5, col6, col7,
        col8, col9, col10, col11, col12, col13, col14
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
        testRow,
      );
    });

    await Promise.all([firstPromise, secondPromise, thirdPromise]);
  }

  console.log("Finished transactions");

  for (let i = 0; i < 100000; i++) {
    db.executeSync("SELECT * FROM perf_table WHERE id = 1");
  }
  const end = performance.now();
  console.log(`Performance test finished 🟢 in ${end - start} ms`);

  // await db.close();
  return end - start;
}
