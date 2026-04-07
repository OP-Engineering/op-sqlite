const fs = require('fs');

// Read the package.json file
const packageJson = JSON.parse(fs.readFileSync('./example/package.json'));

// Enable Turso backend and disable other mutually exclusive backends
packageJson['op-sqlite']['turso'] = true;
packageJson['op-sqlite']['libsql'] = false;
packageJson['op-sqlite']['sqlcipher'] = false;
packageJson['op-sqlite']['iosSqlite'] = false;
packageJson['op-sqlite']['sqliteVec'] = false;
packageJson['op-sqlite']['tokenizers'] = [];

// Save the updated package.json file
fs.writeFileSync(
  './example/package.json',
  JSON.stringify(packageJson, null, 2)
);

console.log('Turned on turso in package.json', packageJson);