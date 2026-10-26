import { type DB, open } from "@op-engineering/op-sqlite";
import {
	afterAll,
	beforeEach,
	describe,
	expect,
	it,
} from "@op-engineering/op-test";

describe("PreparedStatements", () => {
	let db: DB;
	beforeEach(async () => {
		try {
			if (db) {
				db.close();
				db.delete();
			}

			db = open({
				name: "statements",
				encryptionKey: "test",
			});

			await db.execute("DROP TABLE IF EXISTS User;");
			await db.execute(
				"CREATE TABLE User ( id INT PRIMARY KEY, name TEXT) STRICT;",
			);
			await db.execute('INSERT INTO "User" (id, name) VALUES(?,?)', [
				1,
				"Oscar",
			]);
			await db.execute('INSERT INTO "User" (id, name) VALUES(?,?)', [
				2,
				"Pablo",
			]);
			await db.execute('INSERT INTO "User" (id, name) VALUES(?,?)', [
				3,
				"Carlos",
			]);
		} catch (e) {
			console.warn("Prepared statements block, error on before each", e);
		}
	});

	afterAll(() => {
		if (db) {
			db.close();
			db.delete();
			// @ts-expect-error
			db = null;
		}
	});

	it("Creates a prepared statement and executes a prepared statement", async () => {
		const statement = db.prepareStatement("SELECT * FROM User;");
		let results = await statement.execute();

		expect(results.rows?.length).toEqual(3);
		results = await statement.execute();

		expect(results.rows.length).toEqual(3);
	});

	it("prepared statement, rebind select", async () => {
		const statement = db.prepareStatement("SELECT * FROM User WHERE id = ?;");
		await statement.bind([1]);

		let results = await statement.execute();
		expect(results.rows[0]!.name === "Oscar");

		await statement.bind([2]);
		results = await statement.execute();
		expect(results.rows[0]!.name === "Pablo");
	});

	it("prepared statement, rebind insert", async () => {
		const statement = db.prepareStatement(
			'INSERT INTO "User" (id, name) VALUES(?,?);',
		);
		await statement.bind([4, "Juan"]);
		await statement.execute();

		await statement.bind([5, "Pedro"]);
		await statement.execute();
	});

	it("prepared statement, bindsync", async () => {
		const statement = db.prepareStatement(
			'INSERT INTO "User" (id, name) VALUES(?,?);',
		);

		statement.bindSync([4, "Juan"]);
		await statement.execute();

		statement.bind([5, "Pedro"]);
		await statement.execute();

		const selectStatement = db.prepareStatement("SELECT * FROM User;");
		const results = await selectStatement.execute();
		expect(results.rows.length).toEqual(5);
	});

	it("prepared statement, executeSync", () => {
		const statement = db.prepareStatement("SELECT * FROM User;");
		let results = statement.executeSync();

		expect(results.rows?.length).toEqual(3);
		results = statement.executeSync();

		expect(results.rows.length).toEqual(3);
	});

	it("prepared statement, bindsync + executeSync", () => {
		const statement = db.prepareStatement(
			'INSERT INTO "User" (id, name) VALUES(?,?);',
		);

		statement.bindSync([4, "Juan"]);
		statement.executeSync();

		statement.bindSync([5, "Pedro"]);
		statement.executeSync();

		const selectStatement = db.prepareStatement("SELECT * FROM User;");
		const results = selectStatement.executeSync();
		expect(results.rows.length).toEqual(5);
	});

	it("prepared statement, execute after close rejects", async () => {
		const statement = db.prepareStatement("SELECT * FROM User;");
		db.close();

		// Rejects (does not throw synchronously), like db.execute after close
		const promise = statement.execute();
		let error: any;
		try {
			await promise;
		} catch (e) {
			error = e;
		}

		expect(error?.message).toContain(
			"[op-sqlite][execute] database is closed",
		);
		expect(error.code).toEqual(undefined);
	});

	it("prepared statement, sync calls after close throw", () => {
		const statement = db.prepareStatement(
			'INSERT INTO "User" (id, name) VALUES(?,?);',
		);
		db.close();

		let executeError: any;
		try {
			statement.executeSync();
		} catch (e) {
			executeError = e;
		}
		expect(executeError?.message).toContain(
			"[op-sqlite][executeSync] database is closed",
		);
		expect(executeError.code).toEqual(undefined);

		let bindError: any;
		try {
			statement.bindSync([4, "Juan"]);
		} catch (e) {
			bindError = e;
		}
		expect(bindError?.message).toContain(
			"[op-sqlite][bindSync] database is closed",
		);
	});
});
