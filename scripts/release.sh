set -ex

cd ..
npm --no-git-tag-version version patch
VERSION=$(node -p "require('./package.json').version")
echo "Releasing $VERSION"
git add .
git commit -m "Release v$(VERSION)"
# git push 
# git tag "$(VERSION)"
# git push --tags 
# npm publish