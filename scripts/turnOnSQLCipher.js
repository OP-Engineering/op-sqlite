const fs = require('fs');

console.log('Current working directory:', process.cwd());

// Read the package.json file
const packageJson = JSON.parse(fs.readFileSync('package.json'));

// Modify the op-sqlite.sqlcipher key to true
packageJson['op-sqlite']['sqlcipher'] = true;

// Save the updated package.json file
fs.writeFileSync('package.json', JSON.stringify(packageJson, null, 2));

console.log('package.json updated successfully!');