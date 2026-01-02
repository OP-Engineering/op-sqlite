const { open } = require('./node/lib/index');

const db = open({ name: 'test_trans.sqlite', location: './' });

// Create table
db.executeSync('CREATE TABLE IF NOT EXISTS test_users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)');

// Insert initial data
db.executeSync('INSERT INTO test_users (name, age) VALUES (?, ?)', ['Alice', 30]);
db.executeSync('INSERT INTO test_users (name, age) VALUES (?, ?)', ['Bob', 25]);
db.executeSync('INSERT INTO test_users (name, age) VALUES (?, ?)', ['Charlie', 35]);

console.log('Before transaction:');
let result = db.executeSync('SELECT COUNT(*) as count FROM test_users');
console.log('Rows:', result.rows);
console.log('Count:', result.rows[0].count);

// Run transaction
(async () => {
  await db.transaction(async (tx) => {
    await tx.execute('INSERT INTO test_users (name, age) VALUES (?, ?)', ['David', 40]);
    await tx.execute('INSERT INTO test_users (name, age) VALUES (?, ?)', ['Emma', 28]);
  });

  console.log('\nAfter transaction:');
  result = db.executeSync('SELECT COUNT(*) as count FROM test_users');
  console.log('Rows:', result.rows);
  console.log('Count:', result.rows[0].count);

  db.close();
})();
