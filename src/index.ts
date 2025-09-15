import { NativeModules } from 'react-native';

export {
  OPSQLite,
  open,
  openRemote,
  openSync,
  moveAssetsDatabase,
  getDylibPath,
  isSQLCipher,
  isLibsql,
  isIOSEmbedded,
  isIOSEmbeeded,
} from './functions';
export { Storage } from './Storage';
export type {
  Scalar,
  QueryResult,
  ColumnMetadata,
  SQLBatchTuple,
  UpdateHookOperation,
  BatchQueryResult,
  FileLoadResult,
  Transaction,
  _PendingTransaction,
  PreparedStatement,
  _InternalDB,
  DB,
  DBParams,
  OPSQLiteProxy,
} from './types';

export const {
  IOS_DOCUMENT_PATH,
  IOS_LIBRARY_PATH,
  ANDROID_DATABASE_PATH,
  ANDROID_FILES_PATH,
  ANDROID_EXTERNAL_FILES_PATH,
} = !!NativeModules.OPSQLite.getConstants
  ? NativeModules.OPSQLite.getConstants()
  : NativeModules.OPSQLite;
