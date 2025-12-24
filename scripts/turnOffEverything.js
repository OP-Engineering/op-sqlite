const fs = require('fs');

// console.log('Current working directory:', process.cwd());

// Read the package.json file
const packageJson = JSON.parse(fs.readFileSync('./example/package.json'));

// Modify the op-sqlite.sqlcipher key to true
packageJson['op-sqlite']['libsql'] = false;
packageJson['op-sqlite']['sqlcipher'] = false;
packageJson['op-sqlite']['iosSqlite'] = false;
packageJson['op-sqlite']['fts5'] = true;
packageJson['op-sqlite']['rtree'] = true;
packageJson['op-sqlite']['crsqlite'] = false;
packageJson['op-sqlite']['sqliteVec'] = false;

// Save the updated package.json file
fs.writeFileSync(
  './example/package.json',
  JSON.stringify(packageJson, null, 2)
);

console.log('package.json updated successfully!');
