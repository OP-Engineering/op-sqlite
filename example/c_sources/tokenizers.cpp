#include "tokenizers.h"
#include <cctype>
#include <memory>
#include <string>
#include <iostream>
#include "../../cpp/sqlite3.h"

//SQLITE_EXTENSION_INIT1

namespace opsqlite {

fts5_api *fts5_api_from_db(sqlite3 *db){
  fts5_api *pRet = 0;
  sqlite3_stmt *pStmt = 0;

  if( SQLITE_OK == sqlite3_prepare_v2(db, "SELECT fts5(?1)", -1, &pStmt, 0) ){
    sqlite3_bind_pointer(pStmt, 1, (void*)&pRet, "fts5_api_ptr", NULL);
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

// int sqlite_ngram_init(sqlite3 *db, char **error,
//                       const sqlite3_api_routines *api) {}
// int sqlite_edgengram_init(sqlite3 *db, char **error,
//                           const sqlite3_api_routines *api) {}
int sqlite_wordtokenizer_init(sqlite3 *db, char **error,
                                         const sqlite3_api_routines *api) {
//  SQLITE_EXTENSION_INIT2(api);

  // Create the FTS5 tokenizer structure
  fts5_tokenizer wordtokenizer = {wordTokenizerCreate, wordTokenizerDelete,
                                  wordTokenizerTokenize};

  // Register the tokenizer with FTS5
  fts5_api *ftsApi = (fts5_api *)fts5_api_from_db(db);
  if (ftsApi == NULL)
    return SQLITE_ERROR;
    
  return ftsApi->xCreateTokenizer(ftsApi, "wordtokenizer", NULL, &wordtokenizer,
                                  NULL);
}

} // namespace opsqlite
