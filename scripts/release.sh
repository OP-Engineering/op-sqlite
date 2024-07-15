set -ex

cd ..
VERSION=$(node -p "require('./package.json').version")

npm --no-git-tag-version version patch
git add .
git commit -m "Release v$(VERSION)"
git push 
git tag $(VERSION)
git push --tags 
npm publish