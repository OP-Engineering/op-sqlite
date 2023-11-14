#!/bin/zsh

set -ex

rm -rf sqlite_build
rm -f ./cpp/sqlite3.c
rm -f ./cpp/sqlite3.h

mkdir sqlite_build

cd sqlite_build

../sqlcipher/configure

make

make sqlite3.c

cp sqlite3.c ../cpp/sqlite3.c
cp sqlite3.h ../cpp/sqlite3.h