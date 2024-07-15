set -ex

./bump-version.sh
git add ..
git commit -m 'Release v$(node -p \"require('../package.json').version\")' 
git push 
git tag v$(node -p \"require('../package.json').version\") 
git push --tags 
npm publish