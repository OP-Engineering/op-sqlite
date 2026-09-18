import { type DB, isLibsql, isTurso, open } from "@op-engineering/op-sqlite";
import {
	afterEach,
	beforeEach,
	describe,
	expect,
	it,
} from "@op-engineering/op-test";
import Chance from "chance";
import { sleep } from "./utils";

const DB_CONFIG = {
	name: "hooksDb",
	encryptionKey: "test",
};
const chance = new Chance();

describe("Hooks", () => {
	let db: DB;
	if (isLibsql() || isTurso()) {
		return;
	}

	beforeEach(async () => {
		try {
			db = open(DB_CONFIG);

			await db.execute("DROP TABLE IF EXISTS User;");
			await db.execute(
				"CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;",
			);
		} catch (e) {
			console.warn("Hooks Block, error on before each", e);
		}
	});

	afterEach(() => {
		if (db) {
			db.delete();
		}
	});

	it("update hook", async () => {
		let promiseResolve: any;
		const promise = new Promise<{
			rowId: number;
			row?: any;
			operation: string;
			table: string;
		}>((resolve) => {
			promiseResolve = resolve;
		});

		db.updateHook((data) => {
			promiseResolve(data);
		});

		const id = chance.integer();
		const name = chance.name();
		const age = chance.integer();
		const networth = chance.floating();
		await db.transaction(async (tx) => {
			await tx.execute(
				'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
				[id, name, age, networth],
			);
		});

		const data = await promise;

		expect(data.operation).toEqual("INSERT");
		expect(data.rowId).toEqual(1);

		db.updateHook(null);
	});

	it("Execute batch should trigger update hook", async () => {
		const id = chance.integer();
		const name = chance.name();
		const age = chance.integer();
		const networth = chance.floating();

		db.executeSync(
			'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
			[id, name, age, networth],
		);

		let promiseResolve: any;
		const promise = new Promise<{
			rowId: number;
			row?: any;
			operation: string;
			table: string;
		}>((resolve) => {
			promiseResolve = resolve;
		});

		db.updateHook((data) => {
			promiseResolve(data);
		});

		await db.executeBatch([
			['UPDATE "User" SET name = ? WHERE id = ?', ["foo", id]],
		]);

		const data = await promise;

		expect(data.operation).toEqual("UPDATE");
		expect(data.rowId).toEqual(1);
	});

	it("remove update hook", async () => {
		const hookRes: string[] = [];

		db.updateHook(({ operation }) => {
			hookRes.push(operation);
		});

		const id = chance.integer();
		const name = chance.name();
		const age = chance.integer();
		const networth = chance.floating();
		await db.transaction(async (tx) => {
			await tx.execute(
				'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
				[id, name, age, networth],
			);
		});

		db.updateHook(null);

		await db.transaction(async (tx) => {
			await tx.execute(
				'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
				[id + 1, name, age, networth],
			);
		});

		await sleep(0);

		expect(hookRes.length).toEqual(1);
	});

	it("commit hook", async () => {
		let promiseResolve: any;
		const promise = new Promise((resolve) => {
			promiseResolve = resolve;
		});

		db.commitHook(() => {
			promiseResolve?.();
		});

		const id = chance.integer();
		const name = chance.name();
		const age = chance.integer();
		const networth = chance.floating();
		await db.transaction(async (tx) => {
			await tx.execute(
				'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
				[id, name, age, networth],
			);
		});

		await promise;
		db.commitHook(null);
	});

	it("remove commit hook", async () => {
		const hookRes: string[] = [];
		db.commitHook(() => {
			hookRes.push("commit");
		});

		const id = chance.integer();
		const name = chance.name();
		const age = chance.integer();
		const networth = chance.floating();
		await db.transaction(async (tx) => {
			await tx.execute(
				'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
				[id, name, age, networth],
			);
		});

		db.commitHook(null);

		await db.transaction(async (tx) => {
			await tx.execute(
				'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
				[id + 1, name, age, networth],
			);
		});

		await sleep(0);

		expect(hookRes.length).toEqual(1);
	});

	it("rollback hook", async () => {
		let promiseResolve: any;
		const promise = new Promise((resolve) => {
			promiseResolve = resolve;
		});

		db.rollbackHook(() => {
			promiseResolve?.();
		});

		try {
			await db.transaction(async () => {
				throw new Error("Blah");
			});
		} catch (e) {
			// intentionally left blank
		}

		await promise;
	});

	it("remove rollback hook", async () => {
		const hookRes: string[] = [];
		db.rollbackHook(() => {
			hookRes.push("rollback");
		});

		try {
			await db.transaction(async () => {
				throw new Error("Blah");
			});
		} catch (e) {
			// intentionally left blank
		}

		db.rollbackHook(null);

		try {
			await db.transaction(async () => {
				throw new Error("Blah");
			});
		} catch (e) {
			// intentionally left blank
		}

		await sleep(0);

		expect(hookRes.length).toEqual(1);
	});

	// Regression test for a use-after-free: on_commit()/on_rollback() used to
	// capture raw `this` in the invoker->invokeAsync lambda and read
	// commit_hook_callback/rollback_hook_callback (members of OPDatabase) when
	// the lambda finally ran on the JS thread. close()/delete() only drain the
	// native thread pool, not the invoker queue, so a db freed right after the
	// hook fires but before the queued callback runs would leave that lambda
	// dereferencing a dangling OPDatabase. Closing/deleting immediately after
	// triggering the hook - without waiting for its callback to run - recreates
	// that race; the test passing (instead of crashing the process) is the
	// assertion.
	it("does not crash when db is closed immediately after commit hook fires", async () => {
		const raceDb = open({ name: "commitHookTeardownRace.sqlite" });

		raceDb.executeSync(
			"CREATE TABLE IF NOT EXISTS User (id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;",
		);

		raceDb.commitHook(() => {});

		raceDb.executeSync(
			'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
			[
				chance.integer(),
				chance.name(),
				chance.integer(),
				chance.floating(),
			],
		);

		// The commit hook callback above is now queued on the JS invoker but
		// has not run yet.
		raceDb.close();
		raceDb.delete();

		// Give the JS thread a chance to run the still-queued callback.
		await sleep(50);

		expect(true).toEqual(true);
	});

	it("does not crash when db is closed immediately after rollback hook fires", async () => {
		const raceDb = open({ name: "rollbackHookTeardownRace.sqlite" });

		raceDb.executeSync(
			"CREATE TABLE IF NOT EXISTS User (id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;",
		);

		raceDb.rollbackHook(() => {});

		// Drive BEGIN/INSERT/ROLLBACK directly through executeSync (the same
		// statements db.transaction() issues internally) so the rollback hook
		// fires synchronously, in-line, with no `await` giving the JS thread a
		// chance to drain the invoker queue before close()/delete() below.
		raceDb.executeSync("BEGIN TRANSACTION;");
		raceDb.executeSync(
			'INSERT INTO "User" (id, name, age, networth) VALUES(?, ?, ?, ?)',
			[
				chance.integer(),
				chance.name(),
				chance.integer(),
				chance.floating(),
			],
		);
		raceDb.executeSync("ROLLBACK;");

		// The rollback hook callback above is now queued on the JS invoker but
		// has not run yet.
		raceDb.close();
		raceDb.delete();

		// Give the JS thread a chance to run the still-queued callback.
		await sleep(50);

		expect(true).toEqual(true);
	});
});
