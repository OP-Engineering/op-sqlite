import { TurboModuleRegistry, TurboModule } from 'react-native';

export interface Spec extends TurboModule {
  getConstants: () => {
    IOS_DOCUMENT_PATH: string;
    IOS_LIBRARY_PATH: string;
    ANDROID_DATABASE_PATH: string;
    ANDROID_FILES_PATH: string;
    ANDROID_EXTERNAL_FILES_PATH: string;
  };

  install(): boolean;
}

export default TurboModuleRegistry.getEnforcing<Spec>('OPSQLite');
