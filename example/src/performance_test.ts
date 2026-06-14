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

  const testRow = Array(14).fill("test");
  const runDurations: number[] = [];

  for (let run = 0; run < 10; run++) {
    db.executeSync("DELETE FROM perf_table");

    const start = performance.now();

    for (let i = 0; i < 200; i++) {
      const txPromises = Array.from({ length: 10 }, () =>
        db.transaction(async (tx) => {
          await tx.execute(
            `INSERT INTO perf_table (
        col1, col2, col3, col4, col5, col6, col7,
        col8, col9, col10, col11, col12, col13, col14
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
            testRow,
          );
        }),
      );

      await Promise.all(txPromises);
    }

    for (let i = 0; i < 100000; i++) {
      db.executeSync("SELECT * FROM perf_table WHERE id = 1");
    }

    const end = performance.now();
    runDurations.push(end - start);
  }

  const total = runDurations.reduce((sum, duration) => sum + duration, 0);
  const average = total / runDurations.length;

  // await db.close();
  return average;
}
