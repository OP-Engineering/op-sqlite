#!/bin/bash

set -ex

npm --no-git-tag-version version patch

git add .

git commit -m "Bump version"

PACKAGE_VERSION=$(cat package.json | grep version | head -1 | awk -F: '{ print $2 }' | sed 's/[\",]//g' | tr -d '[[:space:]]')

git tag $PACKAGE_VERSION

git push

git push --tags