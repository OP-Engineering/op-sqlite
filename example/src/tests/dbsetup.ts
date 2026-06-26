import {
	ANDROID_DATABASE_PATH,
	// ANDROID_EXTERNAL_FILES_PATH,
	IOS_LIBRARY_PATH,
	isIOSEmbeeded,
	isLibsql,
	isSQLCipher,
	isTurso,
	moveAssetsDatabase,
	open,
} from "@op-engineering/op-sqlite";
import { describe, expect, it } from "@op-engineering/op-test";
import { Platform } from "react-native";

let expectedVersion = "3.51.3";
let flavor = "sqlite";

if (isLibsql()) {
	expectedVersion = "3.45.1";
	flavor = "libsql";
} else if (isTurso()) {
	expectedVersion = "3.50.4";
	flavor = "turso";
} else if (isSQLCipher()) {
	expectedVersion = "3.51.3";
	flavor = "sqlcipher";
}

// const expectedSqliteVecVersion = 'v0.1.2-alpha.7';

describe("DB setup tests", () => {
	// it('Should match the sqlite_vec version', async () => {
	//   let db = open({
	//     name: 'versionTest.sqlite',
	//   });

	//   const res = db.execute('select vec_version();');

	//   expect(res.rows?._array[0]['vec_version()']).to.equal(
	//     expectedSqliteVecVersion,
	//   );

	//   db.close();
	// });

	// Using the embedded version, you can never be sure which version is used
	// It will change from OS version to version
	if (!isIOSEmbeeded()) {
		it(`Should match the sqlite flavor ${flavor} expected version ${expectedVersion}`, async () => {
			const db = open({
				name: "versionTest.sqlite",
				encryptionKey: "test",
			});

			const res = await db.execute("select sqlite_version();");

			expect(res.rows[0]!["sqlite_version()"]).toBe(expectedVersion);
			db.close();
		});
	}

	it("Create in memory DB", async () => {
		const inMemoryDb = open({
			name: "inMemoryTest.sqlite",
			location: ":memory:",
			encryptionKey: "test",
		});

		await inMemoryDb.execute("DROP TABLE IF EXISTS User;");
		await inMemoryDb.execute(
			"CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;",
		);

		inMemoryDb.close();
	});

	// if (Platform.OS === "android") {
	// 	it("Create db in external directory Android", async () => {
	// 		const androidDb = open({
	// 			name: "AndroidSDCardDB.sqlite",
	// 			location: ANDROID_EXTERNAL_FILES_PATH,
	// 			encryptionKey: "test",
	// 		});

	// 		await androidDb.execute("DROP TABLE IF EXISTS User;");
	// 		await androidDb.execute(
	// 			"CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;",
	// 		);

	// 		androidDb.close();
	// 	});

	// 	it("Creates db in external nested directory on Android", async () => {
	// 		const androidDb = open({
	// 			name: "AndroidSDCardDB.sqlite",
	// 			location: `${ANDROID_EXTERNAL_FILES_PATH}/nested`,
	// 			encryptionKey: "test",
	// 		});

	// 		await androidDb.execute("DROP TABLE IF EXISTS User;");
	// 		await androidDb.execute(
	// 			"CREATE TABLE User ( id INT PRIMARY KEY, name TEXT NOT NULL, age INT, networth REAL) STRICT;",
	// 		);

	// 		androidDb.close();
	// 	});
	// }

	// Currently this only tests the function is there
	it("Should load extension", async () => {
		const db = open({
			name: "extensionDb",
			encryptionKey: "test",
		});

		try {
			db.loadExtension("path");
		} catch (e) {
			// TODO load a sample extension
			expect(!!e).toEqual(true);
		} finally {
			db.delete();
		}
	});

	it("Should delete db", async () => {
		const db = open({
			name: "deleteTest",
			encryptionKey: "test",
		});

		db.delete();
	});

	it("Should delete db with absolute path", async () => {
		const location =
			Platform.OS === "ios" ? IOS_LIBRARY_PATH : ANDROID_DATABASE_PATH;
		const db = open({
			name: "deleteTest",
			encryptionKey: "test",
			location,
		});

		expect(db.getDbPath().includes(location)).toEqual(true);

		db.delete();
	});

	it("Should create db in custom folder", async () => {
		const db = open({
			name: "customFolderTest.sqlite",
			encryptionKey: "test",
			location: "myFolder",
		});

		const path = db.getDbPath();
		expect(path.includes("myFolder")).toEqual(true);
		db.delete();
	});

	it("Should create nested folders", async () => {
		const db = open({
			name: "nestedFolderTest.sqlite",
			encryptionKey: "test",
			location: "myFolder/nested",
		});

		const path = db.getDbPath();
		expect(path.includes("myFolder/nested")).toEqual(true);
		db.delete();
	});

	it("Moves assets database simple", async () => {
		const copied = await moveAssetsDatabase({ filename: "sample.sqlite" });

		expect(copied).toEqual(true);
	});

	it("Moves assets database with path", async () => {
		const copied = await moveAssetsDatabase({
			filename: "sample2.sqlite",
			path: "sqlite",
		});

		expect(copied).toEqual(true);
	});

	it("Moves assets database with path and overwrite", async () => {
		const copied = await moveAssetsDatabase({
			filename: "sample2.sqlite",
			path: "sqlite",
			overwrite: true,
		});

		expect(copied).toEqual(true);

		const db = open({
			name: "sample2.sqlite",
			encryptionKey: "test",
			location: "sqlite",
		});

		const path = db.getDbPath();
		expect(path.includes("sqlite/sample2.sqlite")).toEqual(true);
		db.delete();
	});

	it("Creates new connections per query and closes them", async () => {
		for (let i = 0; i < 100; i++) {
			const db = open({
				name: "versionTest.sqlite",
				encryptionKey: "test",
			});

			await db.execute("select 1;");

			db.close();
		}
	});

	it("Closes connections correctly", async () => {
		try {
			const db1 = open({
				name: "closeTest.sqlite",
			});
			expect(!!db1).toBe(true);
			open({
				name: "closeTest.sqlite",
			});
		} catch (e) {
			expect(!!e).toBe(true);
		}
	});

	it("Can open read-only databases", async () => {
		function openReadOnly() {
			return open({
				name: 'ignored',
				location: ":memory:",
				readOnly: true,
			});
		}

		if (isLibsql()) {
			// libsql C bindings don't expose a way to open read-only databases, so the option is not supported.
			try {
				openReadOnly();
				throw new Error('should have failed');
			} catch (e: any) {
				expect(e.message).toContain('libsql does not support read-only databases');
			}

			return;
		}

		const db = openReadOnly();
		expect(!!db).toBe(true);

		try {
			await db.execute('CREATE TABLE foo (bar TEXT);');
		} catch (e: any) {
			expect(e.message).toContain('attempt to write a readonly database');
		}
	});
});

it("Can attach/dettach database", () => {
	if (isTurso()) {
		return;
	}
	const db = open({
		name: "attachTest.sqlite",
		encryptionKey: "test",
	});
	let db2 = open({
		name: "attachTest2.sqlite",
		encryptionKey: "test",
	});
	db2.close();

	db.attach({
		secondaryDbFileName: "attachTest2.sqlite",
		alias: "attach2",
	});

	db.executeSync("DROP TABLE IF EXISTS attach2.test;");
	db.executeSync(
		"CREATE TABLE IF NOT EXISTS attach2.test (id INTEGER PRIMARY KEY);",
	);
	const res = db.executeSync("INSERT INTO attach2.test (id) VALUES (1);");
	expect(!!res).toBe(true);

	db.detach("attach2");

	db.delete();

	db2 = open({
		name: "attachTest2.sqlite",
		encryptionKey: "test",
	});
	db2.delete();
});

it("Neutralizes SQL injection payload in attach alias", () => {
	if (isTurso()) {
		return;
	}
	const db = open({
		name: "attachInjectionTest.sqlite",
		encryptionKey: "test",
	});
	let db2 = open({
		name: "attachInjectionTest2.sqlite",
		encryptionKey: "test",
	});
	db2.close();

	// Pre-fix, `opsqlite_execute` walked remainingStatement, so the trailing
	// `ATTACH ... AS pwned` would have run as a second prepared statement.
	// Post-fix the alias is passed via parameter binding (`ATTACH DATABASE ?
	// AS ?`), so the whole payload is a single TEXT value used as the
	// schema-name; neither `pwned` nor `evil` ends up attached. The
	// `:memory:` target in the payload keeps the proof side-effect-free
	// across sandboxes in case the pre-fix code path is ever reintroduced.
	const maliciousAlias = "evil; ATTACH DATABASE ':memory:' AS pwned; --";

	db.attach({
		secondaryDbFileName: "attachInjectionTest2.sqlite",
		alias: maliciousAlias,
	});

	let pwnedAttached = false;
	try {
		db.executeSync("SELECT 1 FROM pwned.sqlite_master LIMIT 1;");
		pwnedAttached = true;
	} catch {
		pwnedAttached = false;
	}
	expect(pwnedAttached).toBe(false);

	let evilAttached = false;
	try {
		db.executeSync("SELECT 1 FROM evil.sqlite_master LIMIT 1;");
		evilAttached = true;
	} catch {
		evilAttached = false;
	}
	expect(evilAttached).toBe(false);

	db.detach(maliciousAlias);

	db.delete();

	db2 = open({
		name: "attachInjectionTest2.sqlite",
		encryptionKey: "test",
	});
	db2.delete();
});

it("Neutralizes SQL injection payload in attach path", () => {
	if (isTurso()) {
		return;
	}
	// `opsqlite_get_db_path` just concatenates location + filename, so any
	// quote in the filename used to escape the surrounding string literal
	// in `ATTACH DATABASE '...'`. Post-fix the path is passed via parameter
	// binding, so the embedded quote is just data — no SQL involvement.
	const quirkyFileName = "attach'Injection.sqlite";
	const db = open({
		name: "attachPathHostDb.sqlite",
		encryptionKey: "test",
	});
	const db2 = open({
		name: quirkyFileName,
		encryptionKey: "test",
	});
	db2.close();

	db.attach({
		secondaryDbFileName: quirkyFileName,
		alias: "quirky",
	});
	db.executeSync("DROP TABLE IF EXISTS quirky.canary;");
	db.executeSync(
		"CREATE TABLE IF NOT EXISTS quirky.canary (id INTEGER PRIMARY KEY);",
	);
	db.executeSync("INSERT INTO quirky.canary (id) VALUES (1);");
	const rows = db.executeSync("SELECT id FROM quirky.canary;").rows;
	expect(rows[0]!.id).toBe(1);

	db.detach("quirky");
	db.delete();

	open({ name: quirkyFileName, encryptionKey: "test" }).delete();
});

it("Detach with injection payload does not execute trailing SQL", () => {
	if (isTurso()) {
		return;
	}
	const db = open({
		name: "detachInjectionTest.sqlite",
		encryptionKey: "test",
	});
	const secondary = open({
		name: "detachInjectionTest2.sqlite",
		encryptionKey: "test",
	});
	secondary.close();

	db.executeSync("DROP TABLE IF EXISTS canary;");
	db.executeSync("CREATE TABLE canary (id INTEGER PRIMARY KEY);");
	db.executeSync("INSERT INTO canary (id) VALUES (42);");

	// Attach a *real* schema named `safe` so the leading DETACH actually
	// resolves pre-fix. The trailing `DROP TABLE canary; --` would then
	// run as a second prepared statement and the canary row would be gone.
	// Post-fix the alias is passed via parameter binding, so the whole
	// payload is the schema-name to detach; nothing matches. The core
	// proof is that `canary` survives — backends differ on whether a
	// missing-schema DETACH errors (sqlite/sqlcipher) or returns cleanly
	// (libsql), so we don't assert on the throw shape.
	db.attach({
		secondaryDbFileName: "detachInjectionTest2.sqlite",
		alias: "safe",
	});

	try {
		db.detach("safe; DROP TABLE canary; --");
	} catch {
		// Ignored — only the canary check below is load-bearing.
	}

	const rows = db.executeSync("SELECT id FROM canary;").rows;
	expect(rows.length).toBe(1);
	expect(rows[0]!.id).toBe(42);

	// Best-effort cleanup of `safe`. On backends where the malicious
	// detach above did not throw, libsql may also have detached the
	// `safe` schema (treating the bound text as a literal alias-name
	// that didn't match) or left it attached; either way, swallow the
	// error so cleanup doesn't fail the test.
	try {
		db.detach("safe");
	} catch {
		// Already detached or never attached — ignore.
	}
	db.delete();

	open({ name: "detachInjectionTest2.sqlite", encryptionKey: "test" }).delete();
});

if (isSQLCipher()) {
	it("Encryption key with single quote survives a round-trip", () => {
		// Prior code embedded the key directly into `PRAGMA key = '<key>'`,
		// so a quote in the key would either error out or silently set a
		// truncated key. Post-fix the key is set via `sqlite3_key_v2`,
		// which takes a binary buffer + length — preserving the key
		// exactly, so reopening with the same key still decrypts.
		const trickyKey = "p'a''ss\"wrd";
		const dbName = "pragmaKeyInjectionTest.sqlite";

		let db = open({ name: dbName, encryptionKey: trickyKey });
		db.executeSync("DROP TABLE IF EXISTS secret;");
		db.executeSync("CREATE TABLE secret (value TEXT);");
		db.executeSync("INSERT INTO secret (value) VALUES ('ok');");
		db.close();

		db = open({ name: dbName, encryptionKey: trickyKey });
		const rows = db.executeSync("SELECT value FROM secret;").rows;
		expect(rows[0]!.value).toBe("ok");
		db.delete();
	});
}

it("Can get db path", () => {
	const db = open({
		name: "pathTest.sqlite",
		encryptionKey: "test",
	});

	const path = db.getDbPath();
	expect(!!path).toBe(true);
	db.close();
});

if (isLibsql()) {
	it("Libsql can set reserved bytes", async () => {
		const db = open({ name: "test.db" });
		db.setReservedBytes(28);
		expect(db.getReservedBytes()).toEqual(28);
		db.delete();
	});
}

if (isSQLCipher()) {
	it("Can open SQLCipher db without encryption key", () => {
		const db = open({
			name: "pathTest.sqlite",
		});

		db.close();
	});
}
