import { TurboModuleRegistry, TurboModule } from 'react-native';

export interface Spec extends TurboModule {
  install(): boolean;
  clearState(): void;
}

export default TurboModuleRegistry.getEnforcing<Spec>('OPSQLite');
