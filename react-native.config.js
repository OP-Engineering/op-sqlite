// The only place that generates c_sources/tokenizers.h. Also copies the app's
// c_sources into this package so SwiftPM can compile them.
//
// The React Native CLI loads this file, so `npx react-native config` runs it, and
// so do `use_native_modules!` in a Podfile and Gradle's autolinking when the app's
// package.json changes. React Native's SwiftPM scripts load it on every `spm add`,
// `update` and `sync`.
//
// Package.swift can't do this because SwiftPM sandboxes manifest evaluation, and
// spm.autolinkingPlugin isn't an option because a dependency declaring one loses
// its autolinked target.
const fs = require("node:fs");
const path = require("node:path");
const { writeTokenizersHeader } = require("./generate_tokenizers_header_file.js");

function readJSON(file) {
  try {
    return JSON.parse(fs.readFileSync(file, "utf8"));
  } catch {
    return null;
  }
}

function findUp(startDir) {
  let dir = startDir;
  while (true) {
    const candidate = path.join(dir, "package.json");
    if (fs.existsSync(candidate)) {
      return candidate;
    }
    const parent = path.dirname(dir);
    if (parent === dir) {
      return null;
    }
    dir = parent;
  }
}

// The same lookup as op-sqlite.podspec and Package.swift: the app first, which
// these scripts run from or from its ios/ folder, then the old search.
function findConfigPackageJSON() {
  const app = findUp(process.cwd());
  if (app && readJSON(app)?.["op-sqlite"] != null) {
    return app;
  }
  const packageRoot = fs.realpathSync(__dirname);
  if (!packageRoot.includes("node_modules")) {
    return path.join(packageRoot, "example", "package.json");
  }
  return findUp(path.dirname(packageRoot));
}

try {
  const packageJSON = findConfigPackageJSON();
  const tokenizers = packageJSON && readJSON(packageJSON)?.["op-sqlite"]?.tokenizers;
  if (Array.isArray(tokenizers) && tokenizers.length > 0) {
    const cSources = path.join(path.dirname(packageJSON), "c_sources");
    writeTokenizersHeader(tokenizers, path.join(cSources, "tokenizers.h"));

    // Replaced, so a file removed from the app doesn't keep compiling. Keeping the
    // timestamps means an unchanged file doesn't get recompiled.
    const copy = path.join(__dirname, "c_sources");
    fs.rmSync(copy, { recursive: true, force: true });
    fs.cpSync(cSources, copy, { recursive: true, preserveTimestamps: true });
  }
} catch (error) {
  // Never break the config load: the CLI also reads this file for Android, Metro
  // and everything else.
  console.warn(`[OP-SQLITE] could not prepare c_sources: ${error.message}`);
}

module.exports = {};
