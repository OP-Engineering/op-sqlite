#!/bin/sh

set -ex

rm -f ./cpp/sqlcipher/sqlite3.c
rm -f ./cpp/sqlcipher/sqlite3.h

# You need to clone the repo on the parent folder first
cd ../sqlcipher

# You need to install openssl via homebrew. Don't worry it is not really linked against the version you install, it's only needed to generate the header
./configure --enable-tempstore=yes CFLAGS="-DSQLITE_HAS_CODEC -I/opt/homebrew/include" LDFLAGS=-L/opt/homebrew/lib/  

make

make sqlite3.c

cp sqlite3.c ../op-sqlite/cpp/sqlcipher/sqlcipher.c
cp sqlite3.h ../op-sqlite/cpp/sqlcipher/sqlcipher.h