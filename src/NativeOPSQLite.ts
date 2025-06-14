import { TurboModuleRegistry, type TurboModule } from 'react-native';

type MoveAssetsDatabaseParams = {
  filename: string;
  path?: string;
  overwrite?: boolean;
};

export interface Spec extends TurboModule {
  getConstants: () => {
    IOS_DOCUMENT_PATH: string;
    IOS_LIBRARY_PATH: string;
    ANDROID_DATABASE_PATH: string;
    ANDROID_FILES_PATH: string;
    ANDROID_EXTERNAL_FILES_PATH: string;
  };

  install(): string | undefined;

  moveAssetsDatabase(params: MoveAssetsDatabaseParams): Promise<boolean>;
  getDylibPath(bundleId: string, resourceName: string): string;
}

export default TurboModuleRegistry.getEnforcing<Spec>('OPSQLite');
