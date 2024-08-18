set -ex

npm --no-git-tag-version version $1
VERSION=$(node -p "require('./package.json').version")
echo "Releasing $VERSION"
git add .
git commit -m "Release $VERSION"
git push 
git tag "$VERSION"
git push --tags 