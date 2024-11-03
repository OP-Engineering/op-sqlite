#ifndef TOKENIZERS_H
#define TOKENIZERS_H

#include "../../cpp/sqlite3.h"

namespace opsqlite {

int sqlite_wordtokenizer_init(sqlite3 *db, char **error, const sqlite3_api_routines *api);

} // namespace opsqlite

#endif // TOKENIZERS_H
