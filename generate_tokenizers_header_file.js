// Single source of truth for the generated `tokenizers.h`, used by
// react-native.config.js. TOKENIZER_LIST only has to be kept in sync with
// cpp/OPBridge.cpp here.

const fs = require("node:fs");
const path = require("node:path");

/**
 * The expansion of TOKENIZER_LIST is pasted into opsqlite_open(), so `db` and
 * `err_msg` refer to that function's locals.
 */
function generateTokenizersHeader(names) {
  const tokenizerList = names.map((name) => `opsqlite_${name}_init(db,&err_msg,nullptr);`).join("");

  const declarations = names.map(
    (name) =>
      `int opsqlite_${name}_init(sqlite3 *db, char **error, sqlite3_api_routines const *api);`,
  );

  return [
    "#ifndef TOKENIZERS_H",
    "#define TOKENIZERS_H",
    "",
    `#define TOKENIZER_LIST ${tokenizerList}`,
    "",
    "#include <sqlite3.h>",
    "",
    "namespace opsqlite {",
    "",
    ...declarations,
    "",
    "} // namespace opsqlite",
    "",
    "#endif // TOKENIZERS_H",
    "",
  ].join("\n");
}

/**
 * Returns true when the file was written, false when it was already up to date.
 *
 * Rewriting an identical file would bump its mtime and force a recompile of
 * everything that includes it. The React Native CLI loads the config file that
 * calls this for most of its commands.
 */
function writeTokenizersHeader(names, filePath) {
  const contents = generateTokenizersHeader(names);

  if (fs.existsSync(filePath) && fs.readFileSync(filePath, "utf8") === contents) {
    return false;
  }

  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, contents);

  return true;
}

module.exports = { writeTokenizersHeader };
