"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
var _a;
Object.defineProperty(exports, "__esModule", { value: true });
exports.open = exports.ANDROID_EXTERNAL_FILES_PATH = exports.ANDROID_FILES_PATH = exports.ANDROID_DATABASE_PATH = exports.IOS_LIBRARY_PATH = exports.IOS_DOCUMENT_PATH = exports.OPSQLite = void 0;
const NativeOPSQLite_1 = __importDefault(require("./NativeOPSQLite"));
if (global.__OPSQLiteProxy == null) {
    const OPSQLiteModule = NativeOPSQLite_1.default;
    if (OPSQLiteModule == null) {
        throw new Error('Base module not found. Maybe try rebuilding the app.');
    }
    // Check if we are running on-device (JSI)
    if (global.nativeCallSyncHook == null || OPSQLiteModule.install == null) {
        throw new Error('Failed to install op-sqlite: React Native is not running on-device. OPSQLite can only be used when synchronous method invocations (JSI) are possible. If you are using a remote debugger (e.g. Chrome), switch to an on-device debugger (e.g. Flipper) instead.');
    }
    // Call the synchronous blocking install() function
    const result = OPSQLiteModule.install();
    if (result !== true) {
        throw new Error(`Failed to install op-sqlite: The native OPSQLite Module could not be installed! Looks like something went wrong when installing JSI bindings: ${result}`);
    }
    // Check again if the constructor now exists. If not, throw an error.
    if (global.__OPSQLiteProxy == null) {
        throw new Error('Failed to install op-sqlite, the native initializer function does not exist. Are you trying to use OPSQLite from different JS Runtimes?');
    }
}
const proxy = global.__OPSQLiteProxy;
exports.OPSQLite = proxy;
_a = !!NativeOPSQLite_1.default.getConstants
    ? NativeOPSQLite_1.default.getConstants()
    : NativeOPSQLite_1.default, 
// @ts-expect-error
exports.IOS_DOCUMENT_PATH = _a.IOS_DOCUMENT_PATH, 
// @ts-expect-error
exports.IOS_LIBRARY_PATH = _a.IOS_LIBRARY_PATH, 
// @ts-expect-error
exports.ANDROID_DATABASE_PATH = _a.ANDROID_DATABASE_PATH, 
// @ts-expect-error
exports.ANDROID_FILES_PATH = _a.ANDROID_FILES_PATH, 
// @ts-expect-error
exports.ANDROID_EXTERNAL_FILES_PATH = _a.ANDROID_EXTERNAL_FILES_PATH;
const locks = {};
// Enhance some host functions
// Add 'item' function to result object to allow the sqlite-storage typeorm driver to work
function enhanceQueryResult(result) {
    // Add 'item' function to result object to allow the sqlite-storage typeorm driver to work
    if (result.rows == null) {
        result.rows = {
            _array: [],
            length: 0,
            item: (idx) => result.rows?._array[idx],
        };
    }
    else {
        result.rows.item = (idx) => result.rows?._array[idx];
    }
}
const _open = exports.OPSQLite.open;
exports.OPSQLite.open = (dbName, location, encryptionKey) => {
    _open(dbName, location, encryptionKey);
    locks[dbName] = {
        queue: [],
        inProgress: false,
    };
};
const _close = exports.OPSQLite.close;
exports.OPSQLite.close = (dbName) => {
    _close(dbName);
    delete locks[dbName];
};
const _execute = exports.OPSQLite.execute;
exports.OPSQLite.execute = (dbName, query, params) => {
    const sanitizedParams = params?.map((p) => {
        if (ArrayBuffer.isView(p)) {
            return p.buffer;
        }
        return p;
    });
    const result = _execute(dbName, query, sanitizedParams);
    enhanceQueryResult(result);
    return result;
};
const _executeAsync = exports.OPSQLite.executeAsync;
exports.OPSQLite.executeAsync = async (dbName, query, params) => {
    const sanitizedParams = params?.map((p) => {
        if (ArrayBuffer.isView(p)) {
            return p.buffer;
        }
        return p;
    });
    const res = await _executeAsync(dbName, query, sanitizedParams);
    enhanceQueryResult(res);
    return res;
};
exports.OPSQLite.transaction = async (dbName, fn) => {
    if (!locks[dbName]) {
        throw Error(`SQLite Error: No lock found on db: ${dbName}`);
    }
    let isFinalized = false;
    // Local transaction context object implementation
    const execute = (query, params) => {
        if (isFinalized) {
            throw Error(`SQLite Error: Cannot execute query on finalized transaction: ${dbName}`);
        }
        return exports.OPSQLite.execute(dbName, query, params);
    };
    const executeAsync = (query, params) => {
        if (isFinalized) {
            throw Error(`SQLite Error: Cannot execute query on finalized transaction: ${dbName}`);
        }
        return exports.OPSQLite.executeAsync(dbName, query, params);
    };
    const commit = () => {
        if (isFinalized) {
            throw Error(`SQLite Error: Cannot execute commit on finalized transaction: ${dbName}`);
        }
        const result = exports.OPSQLite.execute(dbName, 'COMMIT');
        isFinalized = true;
        return result;
    };
    const rollback = () => {
        if (isFinalized) {
            throw Error(`SQLite Error: Cannot execute rollback on finalized transaction: ${dbName}`);
        }
        const result = exports.OPSQLite.execute(dbName, 'ROLLBACK');
        isFinalized = true;
        return result;
    };
    async function run() {
        try {
            await exports.OPSQLite.executeAsync(dbName, 'BEGIN TRANSACTION');
            await fn({
                commit,
                execute,
                executeAsync,
                rollback,
            });
            if (!isFinalized) {
                commit();
            }
        }
        catch (executionError) {
            if (!isFinalized) {
                try {
                    rollback();
                }
                catch (rollbackError) {
                    throw rollbackError;
                }
            }
            throw executionError;
        }
        finally {
            locks[dbName].inProgress = false;
            isFinalized = false;
            startNextTransaction(dbName);
        }
    }
    return await new Promise((resolve, reject) => {
        const tx = {
            start: () => {
                run().then(resolve).catch(reject);
            },
        };
        locks[dbName].queue.push(tx);
        startNextTransaction(dbName);
    });
};
const startNextTransaction = (dbName) => {
    if (!locks[dbName]) {
        throw Error(`Lock not found for db: ${dbName}`);
    }
    if (locks[dbName].inProgress) {
        // Transaction is already in process bail out
        return;
    }
    if (locks[dbName].queue.length) {
        locks[dbName].inProgress = true;
        const tx = locks[dbName].queue.shift();
        if (!tx) {
            throw new Error('Could not get a operation on datebase');
        }
        setImmediate(() => {
            tx.start();
        });
    }
};
const open = ({ name, location, encryptionKey = '', }) => {
    exports.OPSQLite.open(name, location, encryptionKey);
    return {
        close: () => exports.OPSQLite.close(name),
        delete: () => exports.OPSQLite.delete(name, location),
        attach: (dbNameToAttach, alias, location) => exports.OPSQLite.attach(name, dbNameToAttach, alias, location),
        detach: (alias) => exports.OPSQLite.detach(name, alias),
        transaction: (fn) => exports.OPSQLite.transaction(name, fn),
        execute: (query, params) => exports.OPSQLite.execute(name, query, params),
        executeAsync: (query, params) => exports.OPSQLite.executeAsync(name, query, params),
        executeBatch: (commands) => exports.OPSQLite.executeBatch(name, commands),
        executeBatchAsync: (commands) => exports.OPSQLite.executeBatchAsync(name, commands),
        loadFile: (location) => exports.OPSQLite.loadFile(name, location),
        updateHook: (callback) => exports.OPSQLite.updateHook(name, callback),
        commitHook: (callback) => exports.OPSQLite.commitHook(name, callback),
        rollbackHook: (callback) => exports.OPSQLite.rollbackHook(name, callback),
        prepareStatement: (query) => exports.OPSQLite.prepareStatement(name, query),
    };
};
exports.open = open;
