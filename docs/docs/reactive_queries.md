---
sidebar_position: 5
---

# Reactive Queries

You can subscribe to changes to your database via the use of native queries. Reactivity is achieved through sqlite’s [update hook](https://sqlite.org/c3ref/update_hook.html) and are executed on the native level which means they are blazing fast.

## How reactive queries work

Reactive queries work by re-executing your SQL query when a table or row id are detected to change. Re-running an entire query might be expensive, so internally the query is stored as a prepared statement to optimize the callbacks, there is nothing you need to do for this optimization. The filtering of events is also implemented on C++ which means it is as fast as possible based on the observed tables and rows.

It’s important to notice that due to the dependency on sqlite’s update hook, the row id is not the primary key of the table, but the [row id](https://www.sqlite.org/rowidtable.html) column. If you are using a different primary key, this will not match. You will see in the examples below how to retrieve the corresponding row id for a specific table row.

Most important of all, is that reactive queries are only triggered on transactions due to technical limitations.

## Table queries

You can subscribe to a table being changed, this would be useful whenever you are querying for a list of elements:

```tsx
let unsubscribe = db.reactiveExecute({
  query: 'SELECT * FROM Users',
  fireOn: [
    {
      table: 'User',
    },
  ],
  callback: (usersResponse) => {
    console.log(usersReponse.rows); // should print the entire list of users
    // You can pair this with your favourite state management
    // If you would do this with a mobx store
    runInAction(() => {
      this.users = usersReponse.rows;
    });
  },
});

// If you later want to stop receiving updates or you eliminate the row you are watching
unsubscribe();

// To trigger the reactive query you need to execute a transaction. The query will be re-run
// at the end of the transaction
await db.transaction(async () => {
  await db.execute('...'); // Do a query that mutates the table
});
```

## Row queries

You can also subscribe to specific rows. Here you need to retrieve the row id in order to subscribe to the specific row whenever it updates.

```tsx
let rowid = db
  .execute('SELECT rowid WHERE id = ? FROM Users', [123])
  .item(0).rowid;

let unsubscribe = db.reactiveExecute({
  query: 'SELECT * WHERE id = ? FROM Users',
  arguments: ['123'],
  fireOn: [
    {
      table: 'Users',
      ids: [rowId],
    },
  ],
  callback: (userResponse) => {
    console.log(usersReponse.item(0)); // should print the user whenever it updates
  },
});
```

## Complex queries

The entire query is re-ran every time there is a change detected, so you can use whatever sql statement you want. This operation can be potentially slow but op-sqlite is already heavily optimized to reduce any overhead between the native sqlite response and the JS code possible.

```tsx
let unsubscribe = db.reactiveExecute({
  query: `SELECT 
    c.customer_id,
    c.first_name,
    c.last_name,
    c.email,
    COUNT(o.order_id) AS total_orders,
    SUM(o.total_amount) AS total_spent
FROM 
    customers c
LEFT JOIN 
    orders o ON c.customer_id = o.customer_id
GROUP BY 
    c.customer_id, c.first_name, c.last_name, c.email
ORDER BY 
    total_spent DESC;`,
  arguments: [],
  fireOn: [
    {
      table: 'customers',
    },
    {
      table: 'orders',
    },
  ],
  callback: (data: any) => {
    // data = normal op-sqlite response
  },
});
```
