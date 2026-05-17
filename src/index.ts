import { NativeModules } from "react-native";

export * from "./functions";
export { Storage } from "./Storage";
export type {
	_InternalDB,
	_PendingTransaction,
	BatchQueryResult,
	ColumnMetadata,
	DB,
	DBParams,
	FileLoadResult,
	OPSQLiteProxy,
	PreparedStatement,
	QueryResult,
	Scalar,
	SQLBatchTuple,
	Transaction,
	UpdateHookOperation,
} from "./types";

export const {
	IOS_DOCUMENT_PATH,
	IOS_LIBRARY_PATH,
	ANDROID_DATABASE_PATH,
	ANDROID_FILES_PATH,
	ANDROID_EXTERNAL_FILES_PATH,
} = NativeModules.OPSQLite.getConstants
	? NativeModules.OPSQLite.getConstants()
	: NativeModules.OPSQLite;
