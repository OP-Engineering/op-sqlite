const fs = require('fs');

// Read the package.json file
const packageJson = JSON.parse(fs.readFileSync('./example/package.json'));

// Modify the op-sqlite.sqlcipher key to true
packageJson['op-sqlite']['libsql'] = true;
packageJson['op-sqlite']['sqlcipher'] = false;
packageJson['op-sqlite']['ioSqlite'] = false;
delete packageJson['op-sqlite']['tokenizers'];
packageJson['op-sqlite']['sqliteVec'] = false;

// Save the updated package.json file
fs.writeFileSync(
  './example/package.json',
  JSON.stringify(packageJson, null, 2)
);

console.log('Turned on libsql in package.json', packageJson);
