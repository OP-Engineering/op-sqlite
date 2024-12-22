const fs = require('fs');

// Read the package.json file
const packageJson = JSON.parse(fs.readFileSync('./example/package.json'));

// Modify the op-sqlite.sqlcipher key to true
packageJson['op-sqlite']['iosSqlite'] = true;
packageJson['op-sqlite']['sqlcipher'] = false;
packageJson['op-sqlite']['crsqlite'] = false;
packageJson['op-sqlite']['libsql'] = false;
packageJson['op-sqlite']['sqliteVec'] = false;
packageJson['op-sqlite']['rtree'] = false;
packageJson['op-sqlite']['fts5'] = false;

// Save the updated package.json file
fs.writeFileSync(
  './example/package.json',
  JSON.stringify(packageJson, null, 2)
);

console.log('Turned on ios embedded in package.json', packageJson);
