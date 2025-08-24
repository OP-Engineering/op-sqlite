import {isLibsql, open, type DB} from '@op-engineering/op-sqlite';
import chai from 'chai';
import {afterEach, beforeEach, describe, it} from '@op-engineering/op-test';

const expect = chai.expect;

export function tokenizerTests() {
  let db: DB;

  describe('Tokenizer tests', () => {
    beforeEach(async () => {
      db = open({
        name: 'tokenizers.sqlite',
        encryptionKey: 'test',
      });

      if (!isLibsql()) {
        await db.execute(
          `CREATE VIRTUAL TABLE tokenizer_table USING fts5(content, tokenize = 'wordtokenizer');`,
        );
      }
    });

    afterEach(() => {
      if (db) {
        db.close();
        db.delete();
        // @ts-ignore
        db = null;
      }
    });

    if (!isLibsql()) {
      it('Should match the word split by the tokenizer', async () => {
        await db.execute('INSERT INTO tokenizer_table(content) VALUES (?)', [
          'This is a test document',
        ]);
        const res = await db.execute(
          'SELECT content FROM tokenizer_table WHERE content MATCH ?',
          ['test'],
        );
        expect(res.rows.length).to.be.equal(1);
        expect(res.rows[0]!.content).to.be.equal('This is a test document');
      });
    }
  });
}
