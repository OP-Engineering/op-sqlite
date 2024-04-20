import { TurboModuleRegistry, type TurboModule } from 'react-native';

export interface Spec extends TurboModule {
  getConstants: () => {
    IOS_DOCUMENT_PATH: string;
    IOS_LIBRARY_PATH: string;
    ANDROID_DATABASE_PATH: string;
    ANDROID_FILES_PATH: string;
    ANDROID_EXTERNAL_FILES_PATH: string;
  };

  install(): boolean;

  moveAssetsDatabase(name: string, extension: string): boolean;
}

export default TurboModuleRegistry.getEnforcing<Spec>('OPSQLite');
