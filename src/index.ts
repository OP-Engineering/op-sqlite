import { NativeModules } from 'react-native';

export * from './functions';
export { Storage } from './Storage';
export * from './types';

export const {
  IOS_DOCUMENT_PATH,
  IOS_LIBRARY_PATH,
  ANDROID_DATABASE_PATH,
  ANDROID_FILES_PATH,
  ANDROID_EXTERNAL_FILES_PATH,
  WINDOWS_LOCAL_DATA_PATH,
  WINDOWS_TEMP_PATH,
  WINDOWS_ROAMING_DATA_PATH,
} = !!NativeModules.OPSQLite.getConstants
  ? NativeModules.OPSQLite.getConstants()
  : NativeModules.OPSQLite;
