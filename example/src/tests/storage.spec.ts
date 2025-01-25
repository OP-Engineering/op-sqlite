import {Storage} from '@op-engineering/op-sqlite';
import chai from 'chai';
import {afterEach, beforeEach, describe, it} from './MochaRNAdapter';

const expect = chai.expect;

export function storageTests() {
  let storage: Storage;

  describe('Queries tests', () => {
    beforeEach(async () => {
      storage = new Storage({encryptionKey: 'test'});
    });

    afterEach(() => {});

    it('Can set and get sync', async () => {
      storage.setItemSync('foo', 'bar');
      const res = storage.getItemSync('foo');
      expect(res).to.equal('bar');
    });

    it('Can set and get async', async () => {
      await storage.setItem('quack', 'bark');
      const res = await storage.getItem('quack');
      expect(res).to.equal('bark');
    });

    it('can remove item sync', async () => {
      storage.setItemSync('foo', 'bar');
      storage.removeItemSync('foo');
      const res = storage.getItemSync('foo');
      expect(res).to.equal(undefined);
    });

    it('can remove item async', async () => {
      await storage.setItem('quack', 'bark');
      await storage.removeItem('quack');
      const res = await storage.getItem('quack');
      expect(res).to.equal(undefined);
    });

    it('can clear', async () => {
      await storage.setItem('quack', 'bark');
      await storage.setItem('quack2', 'bark');
      await storage.clear();
      const res = await storage.getItem('quack');
      expect(res).to.equal(undefined);
    });

    it('can clear sync', async () => {
      storage.setItemSync('quack', 'bark');
      storage.setItemSync('quack2', 'bark');
      storage.clearSync();
      const res = storage.getItemSync('quack');
      expect(res).to.equal(undefined);
    });

    it('can get all keys', async () => {
      await storage.setItem('quack', 'bark');
      await storage.setItem('quack2', 'bark');
      const keys = storage.getAllKeys();
      expect(keys).to.deep.equal(['quack', 'quack2']);
    });
  });
}
