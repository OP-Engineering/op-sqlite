import * as path from 'node:path';
import { NodeDatabase } from './database';
import type { DB, DBParams, OPSQLiteProxy } from './types';

export { NodeDatabase as Database } from './database';
export type {
    BatchQueryResult, ColumnMetadata, DB,
    DBParams, FileLoadResult, OPSQLiteProxy, PreparedStatement, QueryResult, Scalar, SQLBatchTuple, Transaction, UpdateHookOperation
} from './types';

class OPSQLiteProxyImpl implements OPSQLiteProxy {
  open(options: {
    name: string;
    location?: string;
    encryptionKey?: string;
  }): DB {
    if (options.encryptionKey) {
      console.warn(
        'Encryption is not supported in the Node.js implementation. Use @journeyapps/sqlcipher for encryption support.'
      );
    }
    return new NodeDatabase(options.name, options.location);
  }

  openV2(options: { path: string; encryptionKey?: string }): DB {
    if (options.encryptionKey) {
      console.warn(
        'Encryption is not supported in the Node.js implementation. Use @journeyapps/sqlcipher for encryption support.'
      );
    }

    const dir = path.dirname(options.path);
    const name = path.basename(options.path);
    return new NodeDatabase(name, dir);
  }

  openRemote(options: { url: string; authToken: string }): DB {
    throw new Error(
      'openRemote is not supported in the Node.js implementation. Use the libsql client directly for remote connections.'
    );
  }

  openSync(options: DBParams): DB {
    throw new Error(
      'openSync is not supported in the Node.js implementation. Use the libsql client directly for sync functionality.'
    );
  }

  isSQLCipher(): boolean {
    return false;
  }

  isLibsql(): boolean {
    return false;
  }

  isIOSEmbedded(): boolean {
    return false;
  }
}

// Create singleton instance
const proxy = new OPSQLiteProxyImpl();

// Export proxy functions for easier usage
export const open = proxy.open.bind(proxy);
export const openV2 = proxy.openV2.bind(proxy);
export const openRemote = proxy.openRemote.bind(proxy);
export const openSync = proxy.openSync.bind(proxy);
export const isSQLCipher = proxy.isSQLCipher.bind(proxy);
export const isLibsql = proxy.isLibsql.bind(proxy);
export const isIOSEmbedded = proxy.isIOSEmbedded.bind(proxy);

// Default export
export default proxy;
