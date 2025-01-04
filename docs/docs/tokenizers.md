---
sidebar_position: 7
---

# Custom Tokenizers

Tokenizers are custom C functions that allow you to turn a stream of characters into “tokens”. Tokens can be anything you want, you can break on whitespaces, special characters, etc. They are meant to help you break the characters for full-text search queries to be more accurate.

op-sqlite has a novel way for you to create your tokenizers.

1. Declare which tokenizers you want on the `package.json`:

   ```json
   "op-sqlite": {
   	// Leave whatever configuration you already have
   	"fts5": true, // fts needs to be enabled
   	"tokenizers": ["word_tokenizer"] // declare which tokenizers you will create
   }
   ```

2. Run `pod install`. The podspec now contains a code generation step. It will create a `c_sources` folder at the root of your project. It will create a `tokenizers.h` file. DON’T TOUCH THIS FILE. It will be overwritten every time. You need to create a `c_sources/tokenizers.cpp` file. Here you need to provide your tokenizer implementation. The `tokenizer.h` file contains the function declaration that will be executed when registering your tokenizer. In this case here is a sample `tokenizers.cpp` implementation

   ```cpp
   #include "tokenizers.h"
   #include <cctype>
   #include <memory>
   #include <string>

   namespace opsqlite {

   fts5_api *fts5_api_from_db(sqlite3 *db) {
     fts5_api *pRet = 0;
     sqlite3_stmt *pStmt = 0;

     if (SQLITE_OK == sqlite3_prepare_v2(db, "SELECT fts5(?1)", -1, &pStmt, 0)) {
       sqlite3_bind_pointer(pStmt, 1, (void *)&pRet, "fts5_api_ptr", NULL);
       sqlite3_step(pStmt);
     }
     sqlite3_finalize(pStmt);
     return pRet;
   }

   class WordTokenizer {
   public:
     WordTokenizer() = default;
     ~WordTokenizer() = default;
   };

   // Define `xCreate`, which initializes the tokenizer
   int wordTokenizerCreate(void *pUnused, const char **azArg, int nArg,
                           Fts5Tokenizer **ppOut) {
     auto tokenizer = std::make_unique<WordTokenizer>();
     *ppOut = reinterpret_cast<Fts5Tokenizer *>(
         tokenizer.release()); // Cast to Fts5Tokenizer*
     return SQLITE_OK;
   }

   // Define `xDelete`, which frees the tokenizer
   void wordTokenizerDelete(Fts5Tokenizer *pTokenizer) {
     delete reinterpret_cast<WordTokenizer *>(pTokenizer);
   }

   // Define `xTokenize`, which performs the actual tokenization
   int wordTokenizerTokenize(Fts5Tokenizer *pTokenizer, void *pCtx, int flags,
                             const char *pText, int nText,
                             int (*xToken)(void *, int, const char *, int, int,
                                           int)) {
     int start = 0;
     int i = 0;

     while (i <= nText) {
       if (i == nText || !std::isalnum(static_cast<unsigned char>(pText[i]))) {
         if (start < i) { // Found a token
           int rc = xToken(pCtx, 0, pText + start, i - start, start, i);
           if (rc != SQLITE_OK)
             return rc;
         }
         start = i + 1;
       }
       i++;
     }
     return SQLITE_OK;
   }

   int opsqlite_word_tokenizer_init(sqlite3 *db, char **error,
                            sqlite3_api_routines const *api) {
     fts5_tokenizer wordtokenizer = {wordTokenizerCreate, wordTokenizerDelete,
                                     wordTokenizerTokenize};

     fts5_api *ftsApi = (fts5_api *)fts5_api_from_db(db);
     if (ftsApi == NULL)
       return SQLITE_ERROR;

     return ftsApi->xCreateTokenizer(ftsApi, "word_tokenizer", NULL,
                                     &wordtokenizer, NULL);
   }

   } // namespace opsqlite

   ```

   You need to keep the namespace and the function signature intact. For now the `sqlite3_api_routines` parameter will always be a null pointer.

3. Once you are done. You need to run `pod install` again. It will then copy the files you created to the pod sources in order to compile `op-sqlite` together with your new C++ code in one go.
4. The code generation step is only implemented in Cocoapods. Every time you create/change a file inside of `c_sources` you will need to do a `pod install` to re-add the newly created files into the compilation process. This also applies for Android, at least the header file generation step. On your CI, you will also need to do a pod install even if your pods are cached, in order to copy the sources.
5. You can then create a FTS5 virtual table with your tokenizer:

   ```tsx
   let db = open({
     name: 'tokenizers.sqlite',
     encryptionKey: 'test',
   });

   // inside your component or wherever you initialize your database
   // THIS IS SAMPLE CODE, use your head when creating your tables
   useEffect(() => {
     let setup = async () => {
       await db.execute(
         `CREATE VIRTUAL TABLE tokenizer_table USING fts5(content, tokenize = 'word_tokenizer');`
       );

       await db.execute('INSERT INTO tokenizer_table(content) VALUES (?)', [
         'This is a test document',
       ]);

       const res = await db.execute(
         'SELECT content FROM tokenizer_table WHERE content MATCH ?',
         ['test']
       );

       console.warn(res);
     };
     setup();
   }, []);
   ```
