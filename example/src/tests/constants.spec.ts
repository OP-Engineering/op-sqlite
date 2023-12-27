import {
  IOS_DOCUMENT_PATH,
  IOS_LIBRARY_PATH,
  ANDROID_EXTERNAL_FILES_PATH,
  ANDROID_DATABASE_PATH,
  ANDROID_FILES_PATH,
} from '@op-engineering/op-sqlcipher';
import chai from 'chai';
import {describe, it} from './MochaRNAdapter';
import {Platform} from 'react-native';

let expect = chai.expect;

export function constantsTests() {
  describe('Constants tests', () => {
    it('Constants are there', async () => {
      if (Platform.OS === 'ios') {
        expect(IOS_DOCUMENT_PATH).to.exist;
        expect(IOS_LIBRARY_PATH).to.exist;
      } else {
        expect(ANDROID_EXTERNAL_FILES_PATH).to.exist;
        expect(ANDROID_DATABASE_PATH).to.exist;
        expect(ANDROID_FILES_PATH).to.exist;
      }
    });
  });
}
