import type {
	_InternalDB,
	_PendingTransaction,
	BatchQueryResult,
	DB,
	DBParams,
	FileLoadResult,
	OPSQLiteProxy,
	PreparedStatement,
	QueryResult,
	Scalar,
	SQLBatchTuple,
	Transaction,
} from "./types";

type WorkerPromiser = (
	type: string,
	args?: Record<string, unknown>,
) => Promise<any>;

const WEB_ONLY_SYNC_ERROR =
	"[op-sqlite] Web backend is async-only. Use openAsync() and async methods like execute().";

function throwSyncApiError(method: string): never {
	throw new Error(`${WEB_ONLY_SYNC_ERROR} Called sync method: ${method}().`);
}

function toNumber(value: unknown): number | undefined {
	if (value == null) {
		return undefined;
	}

	if (typeof value === "bigint") {
		const asNumber = Number(value);
		return Number.isFinite(asNumber) ? asNumber : undefined;
	}

	if (typeof value === "number") {
		return Number.isFinite(value) ? value : undefined;
	}

	return undefined;
}

function ensureSingleStatement(sql: string): void {
	// Web worker executes the full SQL string while native executes only the first prepared statement.
	// We warn here so callers can keep behavior consistent across platforms when needed.
	if (sql.includes(";")) {
		const trimmed = sql.trim();
		if (!trimmed.endsWith(";") || trimmed.slice(0, -1).includes(";")) {
			console.warn(
				"[op-sqlite] Web execute() runs full SQL strings. Avoid multi-statement SQL for parity with native first-statement behavior.",
			);
		}
	}
}

let promiserPromise: Promise<WorkerPromiser> | null = null;

async function getPromiser(): Promise<WorkerPromiser> {
	if (!promiserPromise) {
		promiserPromise = (async () => {
			let mod: any;

			try {
				mod = await import("@sqlite.org/sqlite-wasm");
			} catch (error) {
				throw new Error(
					`[op-sqlite] Web support requires optional dependency @sqlite.org/sqlite-wasm. Install it in your app: npm i @sqlite.org/sqlite-wasm (or yarn add @sqlite.org/sqlite-wasm). Original error: ${(error as Error).message}`,
				);
			}

			const makePromiser = mod.sqlite3Worker1Promiser as any;

			const maybePromiser = makePromiser();

			if (typeof maybePromiser === "function") {
				return maybePromiser as WorkerPromiser;
			}

			return (await maybePromiser) as WorkerPromiser;
		})();
	}

	return promiserPromise;
}

async function ensureOpfs(promiser: WorkerPromiser): Promise<void> {
	const config = await promiser("config-get", {});
	const vfsList = config?.result?.vfsList;

	if (!Array.isArray(vfsList) || !vfsList.includes("opfs")) {
		throw new Error(
			"[op-sqlite] OPFS is required on web for persistence. Ensure COOP/COEP headers are set and OPFS is available in this browser.",
		);
	}
}

async function executeWorker(
	promiser: WorkerPromiser,
	dbId: string,
	query: string,
	params?: Scalar[],
): Promise<QueryResult> {
	ensureSingleStatement(query);

	const response = await promiser("exec", {
		dbId,
		sql: query,
		bind: params,
		rowMode: "object",
		resultRows: [],
		columnNames: [],
		returnValue: "resultRows",
	});

	const result = response?.result;
	const rows = Array.isArray(result?.resultRows)
		? (result.resultRows as Array<Record<string, Scalar>>)
		: Array.isArray(result)
			? (result as Array<Record<string, Scalar>>)
			: [];
	const columnNames = Array.isArray(result?.columnNames)
		? (result.columnNames as string[])
		: rows.length > 0
			? Object.keys(rows[0] ?? {})
			: [];

	const rowsAffected = toNumber(result?.changeCount) ?? 0;
	const insertId = toNumber(result?.lastInsertRowId);

	return {
		rowsAffected,
		insertId,
		rows,
		columnNames,
	};
}

function enhanceWebDb(
	db: _InternalDB,
	options: { name?: string; location?: string },
): DB {
	const lock = {
		queue: [] as _PendingTransaction[],
		inProgress: false,
	};

	const startNextTransaction = () => {
		if (lock.inProgress || lock.queue.length === 0) {
			return;
		}

		lock.inProgress = true;
		const tx = lock.queue.shift();
		if (!tx) {
			throw new Error("Could not get an operation on database");
		}

		setTimeout(() => {
			tx.start();
		}, 0);
	};

	const withTransactionLock = async <T>(work: () => Promise<T>): Promise<T> => {
		return new Promise<T>((resolve, reject) => {
			const tx: _PendingTransaction = {
				start: () => {
					work()
						.then(resolve)
						.catch(reject)
						.finally(() => {
							lock.inProgress = false;
							startNextTransaction();
						});
				},
			};

			lock.queue.push(tx);
			startNextTransaction();
		});
	};

	const unsupported = (method: string) => () => throwSyncApiError(method);

	const enhancedDb: DB = {
		close: unsupported("close"),
		closeAsync: async () => {
			await db.closeAsync?.();
		},
		delete: unsupported("delete"),
		attach: unsupported("attach"),
		detach: unsupported("detach"),
		transaction: async (
			fn: (tx: Transaction) => Promise<void>,
		): Promise<void> => {
			return withTransactionLock(async () => {
				let finalized = false;

				const commit = async (): Promise<QueryResult> => {
					if (finalized) {
						throw new Error(
							`OP-Sqlite Error: Database: ${options.name}. Cannot execute query on finalized transaction`,
						);
					}

					const res = await enhancedDb.execute("COMMIT;");
					finalized = true;
					return res;
				};

				const rollback = (): QueryResult => {
					throwSyncApiError("rollback");
				};

				const execute = async (query: string, params?: Scalar[]) => {
					if (finalized) {
						throw new Error(
							`OP-Sqlite Error: Database: ${options.name}. Cannot execute query on finalized transaction`,
						);
					}

					return enhancedDb.execute(query, params);
				};

				await enhancedDb.execute("BEGIN TRANSACTION;");

				try {
					await fn({
						execute,
						commit,
						rollback,
					});

					if (!finalized) {
						await commit();
					}
				} catch (error) {
					if (!finalized) {
						await enhancedDb.execute("ROLLBACK;");
					}

					throw error;
				}
			});
		},
		executeSync: unsupported("executeSync"),
		execute: db.execute,
		executeWithHostObjects: db.execute,
		executeBatch: async (
			commands: SQLBatchTuple[],
		): Promise<BatchQueryResult> => {
			await withTransactionLock(async () => {
				await db.execute("BEGIN TRANSACTION;");

				try {
					for (const command of commands) {
						const [sql, bind] = command;

						if (!bind) {
							await db.execute(sql);
							continue;
						}

						if (Array.isArray(bind[0])) {
							for (const rowBind of bind as Scalar[][]) {
								await db.execute(sql, rowBind);
							}
						} else {
							await db.execute(sql, bind as Scalar[]);
						}
					}

					await db.execute("COMMIT;");
				} catch (error) {
					await db.execute("ROLLBACK;");
					throw error;
				}
			});

			return {
				rowsAffected: 0,
			};
		},
		loadFile: async (_location: string): Promise<FileLoadResult> => {
			throw new Error("[op-sqlite] loadFile() is not supported on web.");
		},
		updateHook: () => {
			throw new Error("[op-sqlite] updateHook() is not supported on web.");
		},
		commitHook: () => {
			throw new Error("[op-sqlite] commitHook() is not supported on web.");
		},
		rollbackHook: () => {
			throw new Error("[op-sqlite] rollbackHook() is not supported on web.");
		},
		prepareStatement: (query: string): PreparedStatement => {
			let currentParams: Scalar[] = [];

			return {
				bind: async (params: Scalar[]) => {
					currentParams = params;
				},
				bindSync: unsupported("bindSync"),
				execute: async () => {
					return db.execute(query, currentParams);
				},
			};
		},
		loadExtension: unsupported("loadExtension"),
		executeRaw: db.executeRaw,
		executeRawSync: unsupported("executeRawSync"),
		getDbPath: unsupported("getDbPath"),
		reactiveExecute: unsupported("reactiveExecute"),
		sync: unsupported("sync"),
		setReservedBytes: unsupported("setReservedBytes"),
		getReservedBytes: unsupported("getReservedBytes"),
		flushPendingReactiveQueries: async () => {},
	};

	return enhancedDb;
}

async function createWebDb(params: {
	name: string;
	location?: string;
	encryptionKey?: string;
}): Promise<_InternalDB> {
	if (params.encryptionKey) {
		throw new Error("[op-sqlite] SQLCipher is not supported on web.");
	}

	const promiser = await getPromiser();
	await ensureOpfs(promiser);

	const filename = `file:${params.name}?vfs=opfs`;
	const opened = await promiser("open", {
		filename,
	});

	const dbId = opened?.dbId || opened?.result?.dbId;
	if (!dbId || typeof dbId !== "string") {
		throw new Error("[op-sqlite] Failed to open web sqlite database.");
	}

	return {
		close: () => {
			throwSyncApiError("close");
		},
		closeAsync: async () => {
			await promiser("close", {
				dbId,
			});
		},
		delete: () => {
			throwSyncApiError("delete");
		},
		attach: () => {
			throw new Error("[op-sqlite] attach() is not supported on web.");
		},
		detach: () => {
			throw new Error("[op-sqlite] detach() is not supported on web.");
		},
		transaction: async () => {
			throw new Error(
				"[op-sqlite] transaction() must be called on an opened DB object.",
			);
		},
		executeSync: () => {
			throwSyncApiError("executeSync");
		},
		execute: async (query: string, bind?: Scalar[]) => {
			return executeWorker(promiser, dbId, query, bind);
		},
		executeWithHostObjects: async (query: string, bind?: Scalar[]) => {
			return executeWorker(promiser, dbId, query, bind);
		},
		executeBatch: async (_commands: SQLBatchTuple[]) => {
			throw new Error(
				"[op-sqlite] executeBatch() must be called on an opened DB object.",
			);
		},
		loadFile: async (_location: string) => {
			throw new Error("[op-sqlite] loadFile() is not supported on web.");
		},
		updateHook: () => {
			throw new Error("[op-sqlite] updateHook() is not supported on web.");
		},
		commitHook: () => {
			throw new Error("[op-sqlite] commitHook() is not supported on web.");
		},
		rollbackHook: () => {
			throw new Error("[op-sqlite] rollbackHook() is not supported on web.");
		},
		prepareStatement: (_query: string) => {
			throw new Error(
				"[op-sqlite] prepareStatement() must be called on an opened DB object.",
			);
		},
		loadExtension: () => {
			throw new Error("[op-sqlite] loadExtension() is not supported on web.");
		},
		executeRaw: async (query: string, bind?: Scalar[]) => {
			ensureSingleStatement(query);

			const response = await promiser("exec", {
				dbId,
				sql: query,
				bind,
				rowMode: "array",
				resultRows: [],
				returnValue: "resultRows",
			});

			const result = response?.result;
			const rows = result?.resultRows ?? result;
			return Array.isArray(rows) ? rows : [];
		},
		executeRawSync: () => {
			throwSyncApiError("executeRawSync");
		},
		getDbPath: () => {
			throwSyncApiError("getDbPath");
		},
		reactiveExecute: () => {
			throw new Error("[op-sqlite] reactiveExecute() is not supported on web.");
		},
		sync: () => {
			throwSyncApiError("sync");
		},
		setReservedBytes: () => {
			throwSyncApiError("setReservedBytes");
		},
		getReservedBytes: () => {
			throwSyncApiError("getReservedBytes");
		},
		flushPendingReactiveQueries: async () => {},
	};
}

/**
 * Open a connection to a local sqlite database on web.
 * Web is async-only: use openAsync() and async methods like execute().
 */
export const openAsync = async (params: {
	name: string;
	location?: string;
	encryptionKey?: string;
}): Promise<DB> => {
	const db = await createWebDb(params);
	return enhanceWebDb(db, params);
};

export const open = (_params: {
	name: string;
	location?: string;
	encryptionKey?: string;
}): DB => {
	throwSyncApiError("open");
};

export const openSync = (_params: {
	url: string;
	authToken: string;
	name: string;
	location?: string;
	libsqlSyncInterval?: number;
	libsqlOffline?: boolean;
	encryptionKey?: string;
	remoteEncryptionKey?: string;
}): DB => {
	throwSyncApiError("openSync");
};

export const openRemote = (_params: { url: string; authToken: string }): DB => {
	throw new Error("[op-sqlite] openRemote() is not supported on web.");
};

export const moveAssetsDatabase = async (_args: {
	filename: string;
	path?: string;
	overwrite?: boolean;
}): Promise<boolean> => {
	throw new Error("[op-sqlite] moveAssetsDatabase() is not supported on web.");
};

export const getDylibPath = (_bundle: string, _name: string): string => {
	throw new Error("[op-sqlite] getDylibPath() is not supported on web.");
};

export const isSQLCipher = (): boolean => {
	return false;
};

export const isLibsql = (): boolean => {
	return false;
};

export const isTurso = (): boolean => {
	return false;
};

export const isIOSEmbedded = (): boolean => {
	return false;
};

/**
 * @deprecated Use `isIOSEmbedded` instead. This alias will be removed in a future release.
 */
export const isIOSEmbeeded = isIOSEmbedded;

// Web does not expose the native JSI proxy object.
export const OPSQLite = {} as OPSQLiteProxy;
