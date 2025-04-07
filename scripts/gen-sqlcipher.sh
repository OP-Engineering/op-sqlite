#!/bin/sh

set -ex

rm -f ./cpp/sqlcipher/sqlite3.c
rm -f ./cpp/sqlcipher/sqlite3.h

# You need to clone the repo on the parent folder first
cd ../sqlcipher

# Add the include directory for OpenSSL headers
./configure --with-tempstore=yes CFLAGS="-DSQLITE_HAS_CODEC -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_EXTRA_INIT=sqlcipher_extra_init -DSQLITE_EXTRA_SHUTDOWN=sqlcipher_extra_shutdown -I/opt/homebrew/include" LDFLAGS="-L/opt/homebrew/lib/ -lcrypto"

make

make sqlite3.c

# Fix the path - it should be opsqlite not op-sqlite
cp sqlite3.c ../opsqlite/cpp/sqlcipher/sqlite3.c
cp sqlite3.h ../opsqlite/cpp/sqlcipher/sqlite3.h