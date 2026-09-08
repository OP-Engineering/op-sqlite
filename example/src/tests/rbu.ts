import {
	ANDROID_DATABASE_PATH,
	applyRBU,
	IOS_LIBRARY_PATH,
	isIOSEmbedded,
	isLibsql,
	isRBUEnabled,
	isSQLCipher,
	isTurso,
	open,
} from "@op-engineering/op-sqlite";
import { describe, expect, it } from "@op-engineering/op-test";
import { Platform } from "react-native";

const directory =
	Platform.OS === "ios" ? IOS_LIBRARY_PATH : ANDROID_DATABASE_PATH;

function databasePath(name: string): string {
	return `${directory}/${name}`;
}

function removeDatabase(name: string): void {
	const database = open({ name, location: directory });
	database.delete();
}

async function captureError(
	work: () => Promise<unknown>,
): Promise<{ code?: number; message: string }> {
	try {
		await work();
		return { message: "" };
	} catch (error) {
		return error as { code?: number; message: string };
	}
}

describe("Resumable bulk updates", () => {
	const unsupportedBackend =
		isSQLCipher() || isLibsql() || isTurso() || isIOSEmbedded();

	it("reports whether RBU is compiled into the active backend", () => {
		if (unsupportedBackend) {
			expect(isRBUEnabled()).toEqual(false);
		} else {
			expect(typeof isRBUEnabled()).toEqual("boolean");
		}
	});

	if (!isRBUEnabled()) {
		it("rejects applyRBU when the backend does not provide RBU", async () => {
			const error = await captureError(() =>
				applyRBU({
					targetPath: "/missing-target",
					updatePath: "/missing-update",
				}),
			);

			expect(error.code).toEqual(21);
			expect(error.message.includes("RBU is unavailable")).toEqual(true);
		});
		return;
	}

	it("pauses into a separate state database, resumes, and completes atomically", async () => {
		const targetName = "rbu-target.sqlite";
		const updateName = "rbu-update.sqlite";
		const stateName = "rbu-state.sqlite";

		removeDatabase(targetName);
		removeDatabase(updateName);
		removeDatabase(stateName);

		const target = open({ name: targetName, location: directory });
		await target.execute(
			"CREATE TABLE items (id INTEGER PRIMARY KEY, value TEXT NOT NULL)",
		);
		await target.execute("INSERT INTO items (id, value) VALUES (?, ?)", [
			1,
			"before",
		]);
		target.close();

		const update = open({ name: updateName, location: directory });
		await update.execute(
			"CREATE TABLE data_items (id INTEGER, value TEXT, rbu_control)",
		);
		await update.execute("INSERT INTO data_items VALUES (?, ?, ?)", [
			1,
			"after",
			".x",
		]);
		await update.execute(`
			WITH RECURSIVE sequence(id) AS (
				VALUES(2)
				UNION ALL
				SELECT id + 1 FROM sequence WHERE id < 1202
			)
			INSERT INTO data_items
			SELECT id, 'inserted', 0 FROM sequence
		`);
		update.close();

		const options = {
			targetPath: databasePath(targetName),
			updatePath: databasePath(updateName),
			statePath: databasePath(stateName),
		};

		const paused = await applyRBU({ ...options, maxSteps: 1 });
		expect(paused.status).toEqual("paused");
		expect(paused.steps).toEqual(1);
		expect(paused.progress >= 0).toEqual(true);

		const state = open({
			name: stateName,
			location: directory,
			failOnCreate: true,
		});
		state.close();

		const beforeResume = open({
			name: targetName,
			location: directory,
			readOnly: true,
		});
		const beforeRows = await beforeResume.execute(
			"SELECT value FROM items WHERE id = 1",
		);
		expect(beforeRows.rows[0]?.value).toEqual("before");
		beforeResume.close();

		let complete = await applyRBU(options);
		expect(complete.status).toEqual("paused");
		expect(complete.steps).toEqual(1000);

		let resumeCalls = 0;
		while (complete.status === "paused" && resumeCalls < 10) {
			complete = await applyRBU(options);
			resumeCalls += 1;
		}
		expect(complete.status).toEqual("complete");
		expect(complete.state).toEqual("done");
		expect(complete.steps > 0).toEqual(true);

		const afterResume = open({
			name: targetName,
			location: directory,
			readOnly: true,
		});
		const afterRows = await afterResume.execute(
			"SELECT value FROM items WHERE id = 1",
		);
		expect(afterRows.rows[0]?.value).toEqual("after");
		const compileOption = await afterResume.execute(
			"SELECT sqlite_compileoption_used('ENABLE_RBU') AS enabled",
		);
		expect(compileOption.rows[0]?.enabled).toEqual(1);
		afterResume.close();

		const alreadyComplete = await applyRBU(options);
		expect(alreadyComplete.status).toEqual("complete");
		expect(alreadyComplete.state).toEqual("done");

		removeDatabase(targetName);
		removeDatabase(updateName);
		removeDatabase(stateName);
	});

	it("rejects malformed options before entering native code", async () => {
		const error = await captureError(() =>
			applyRBU({ targetPath: "/target", updatePath: "/update", maxSteps: 0 }),
		);
		expect(error.code).toEqual(21);
		expect(error.message.includes("maxSteps")).toEqual(true);
	});

	it("returns a structured error for a missing target", async () => {
		const updateName = "rbu-missing-target-update.sqlite";
		removeDatabase(updateName);
		const update = open({ name: updateName, location: directory });
		await update.execute(
			"CREATE TABLE data_items (id INTEGER, value TEXT, rbu_control)",
		);
		update.close();

		const error = await captureError(() =>
			applyRBU({
				targetPath: databasePath("rbu-does-not-exist.sqlite"),
				updatePath: databasePath(updateName),
			}),
		);
		expect(error.code).toEqual(14);
		expect(error.message.includes("target database")).toEqual(true);
		removeDatabase(updateName);
	});

	it("returns a structured error for a missing update", async () => {
		const targetName = "rbu-missing-update-target.sqlite";
		removeDatabase(targetName);
		const target = open({ name: targetName, location: directory });
		await target.execute(
			"CREATE TABLE items (id INTEGER PRIMARY KEY, value TEXT)",
		);
		target.close();

		const error = await captureError(() =>
			applyRBU({
				targetPath: databasePath(targetName),
				updatePath: databasePath("rbu-does-not-exist.sqlite"),
			}),
		);
		expect(error.code).toEqual(14);
		expect(error.message.includes("RBU update database")).toEqual(true);
		removeDatabase(targetName);
	});

	it("returns SQLite's error for a malformed RBU database", async () => {
		const targetName = "rbu-invalid-target.sqlite";
		const updateName = "rbu-invalid-update.sqlite";
		removeDatabase(targetName);
		removeDatabase(updateName);

		const target = open({ name: targetName, location: directory });
		await target.execute(
			"CREATE TABLE items (id INTEGER PRIMARY KEY, value TEXT)",
		);
		target.close();
		const update = open({ name: updateName, location: directory });
		await update.execute("CREATE TABLE data_items (id INTEGER, value TEXT)");
		update.close();

		const error = await captureError(() =>
			applyRBU({
				targetPath: databasePath(targetName),
				updatePath: databasePath(updateName),
			}),
		);
		expect(typeof error.code).toEqual("number");
		expect(error.message.length > 0).toEqual(true);

		removeDatabase(targetName);
		removeDatabase(updateName);
	});
});
