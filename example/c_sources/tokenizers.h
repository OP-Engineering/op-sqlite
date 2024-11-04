#ifndef TOKENIZERS_H
#define TOKENIZERS_H

#define TOKENIZER_LIST opsqlite_wordtokenizer_init(db,&errMsg,nullptr);opsqlite_porter_init(db,&errMsg,nullptr);

#include "../../cpp/sqlite3.h"

namespace opsqlite {

int opsqlite_wordtokenizer_init(sqlite3 *db, char **error, const sqlite3_api_routines *api);
int opsqlite_porter_init(sqlite3 *db, char **error, const sqlite3_api_routines *api);

} // namespace opsqlite

#endif // TOKENIZERS_H
