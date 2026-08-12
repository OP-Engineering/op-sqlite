// swift-tools-version: 6.0
import Foundation
import PackageDescription

// Hand-authored SPM manifest for React Native's experimental SwiftPM support
// (RN 0.87, "Preview" — see scripts/spm/__doc__/spm-scripts.md in react-native).
// This is a direct translation of the logic in `op-sqlite.podspec` +
// `generate_tokenizers_header_file.rb`: CocoaPods evaluates that podspec (and
// its dynamic per-consumer-app configuration) as Ruby during `pod install`.
// SwiftPM evaluates *this* file as Swift on every resolve, so the same
// dynamic logic — reading the consumer app's package.json, generating
// cpp/tokenizers.h, choosing which SQLite backend & xcframeworks to link —
// is reproduced here with Foundation instead of Ruby's FileUtils/JSON.
//
// Per RN's "hand-authored community library contract"
// (spm-header-paths-contract.md): a self-managed Package.swift depends on
// the app-local ReactNative + React-GeneratedCode packages by plain relative
// path. The autolinker always exposes a self-managed dep through a symlink
// at `<appRoot>/build/generated/autolinking/libs/<SwiftName>/` — a fixed
// depth from appRoot — so those relative paths are stable constants, not
// something this manifest has to discover at resolve time.

// MARK: - Locate this package + the consuming app's package.json
// Mirrors op-sqlite.podspec's `is_user_app` / package.json discovery.

let packageRoot = URL(fileURLWithPath: #filePath).deletingLastPathComponent()

// In the sample app the dir is not inside of node_modules.
let isUserApp = packageRoot.path.contains("node_modules")

func readJSONObject(atPath path: String) -> [String: Any] {
  guard let data = FileManager.default.contents(atPath: path),
    let object = try? JSONSerialization.jsonObject(with: data),
    let dict = object as? [String: Any]
  else {
    return [:]
  }
  return dict
}

// When installed into node_modules, find the consumer's package.json by
// searching up through parent directories (starting one level above this
// package, i.e. outside node_modules/@op-engineering/op-sqlite). In the
// monorepo/dev checkout it's simply example/package.json.
let appPackageJSONPath: String = {
  guard isUserApp else {
    return packageRoot.appendingPathComponent("example/package.json").path
  }
  var currentDir = packageRoot.deletingLastPathComponent()
  while true {
    let candidate = currentDir.appendingPathComponent("package.json").path
    if FileManager.default.fileExists(atPath: candidate) {
      return candidate
    }
    let parentDir = currentDir.deletingLastPathComponent()
    if parentDir.path == currentDir.path {
      fatalError("[OP-SQLITE] package.json not found")
    }
    currentDir = parentDir
  }
}()

let appPackage = readJSONObject(atPath: appPackageJSONPath)
let opsqliteConfig = appPackage["op-sqlite"] as? [String: Any] ?? [:]

// MARK: - Read op-sqlite config (mirrors op-sqlite.podspec)

let useSqlcipher = (opsqliteConfig["sqlcipher"] as? Bool) == true
let useCrsqlite = (opsqliteConfig["crsqlite"] as? Bool) == true
let useLibsql = (opsqliteConfig["libsql"] as? Bool) == true
let useTurso = (opsqliteConfig["turso"] as? Bool) == true
let performanceMode = (opsqliteConfig["performanceMode"] as? Bool) ?? false
let phoneVersion = (opsqliteConfig["iosSqlite"] as? Bool) == true
let sqliteFlags = (opsqliteConfig["sqliteFlags"] as? String) ?? ""
let fts5 = (opsqliteConfig["fts5"] as? Bool) == true
let rtree = (opsqliteConfig["rtree"] as? Bool) == true
let useSqliteVec = (opsqliteConfig["sqliteVec"] as? Bool) == true
let tokenizers = (opsqliteConfig["tokenizers"] as? [String]) ?? []

if phoneVersion {
  if useSqlcipher {
    fatalError("[OP-SQLITE] SQLCipher is not supported with phone version. It cannot load extensions.")
  }
  if useCrsqlite {
    fatalError("[OP-SQLITE] CRSQLite is not supported with phone version. It cannot load extensions.")
  }
  if rtree {
    fatalError("[OP-SQLITE] RTree is not supported with phone version. It cannot load extensions.")
  }
  if useSqliteVec {
    fatalError("[OP-SQLITE] sqlite-vec is not supported with phone version. It cannot load extensions.")
  }
}
if useLibsql && useSqliteVec {
  fatalError("[OP-SQLITE] You cannot use sqlite-vec with libsql. libsql already has vector search included.")
}
if useLibsql && useCrsqlite {
  fatalError("[OP-SQLITE] You cannot use crsqlite with libsql.")
}
if useTurso && useSqliteVec {
  fatalError("[OP-SQLITE] You cannot use sqlite-vec with turso backend.")
}
if useTurso && useLibsql {
  fatalError("[OP-SQLITE] You cannot enable both libsql and turso backend.")
}
if !tokenizers.isEmpty && useTurso {
  fatalError("[OP-SQLITE] Tokenizers are not supported with turso backend. Please disable tokenizers or do not enable turso.")
}

print("[OP-SQLITE] SPM configuration found at \(appPackageJSONPath)")

// MARK: - Tokenizer header generation
// Direct translation of generate_tokenizers_header_file.rb.

func generateTokenizersHeaderFile(names: [String], filePath: String) {
  let fileURL = URL(fileURLWithPath: filePath)
  try? FileManager.default.createDirectory(
    at: fileURL.deletingLastPathComponent(), withIntermediateDirectories: true)

  let tokenizerList = names.map { "opsqlite_\($0)_init(db,&errMsg,nullptr);" }.joined()

  var content = ""
  content += "#ifndef TOKENIZERS_H\n"
  content += "#define TOKENIZERS_H\n"
  content += "\n"
  content += "#define TOKENIZER_LIST \(tokenizerList)\n"
  content += "\n"
  content += "#include <sqlite3.h>\n"
  content += "\n"
  content += "namespace opsqlite {\n"
  content += "\n"
  for name in names {
    content += "int opsqlite_\(name)_init(sqlite3 *db, char **error, sqlite3_api_routines const *api);\n"
  }
  content += "\n"
  content += "} // namespace opsqlite\n"
  content += "\n"
  content += "#endif // TOKENIZERS_H\n"

  try? content.write(to: fileURL, atomically: true, encoding: .utf8)
}

// Mirrors FileUtils.cp_r: copies the contents of `source` into `destination`,
// creating directories as needed and overwriting existing files.
func copyDirectoryContents(from source: URL, to destination: URL) {
  let fm = FileManager.default
  guard let enumerator = fm.enumerator(at: source, includingPropertiesForKeys: [.isDirectoryKey]) else {
    return
  }
  try? fm.createDirectory(at: destination, withIntermediateDirectories: true)
  for case let itemURL as URL in enumerator {
    let relative = itemURL.path.replacingOccurrences(of: source.path + "/", with: "")
    let destURL = destination.appendingPathComponent(relative)
    let isDirectory = (try? itemURL.resourceValues(forKeys: [.isDirectoryKey]))?.isDirectory ?? false
    if isDirectory {
      try? fm.createDirectory(at: destURL, withIntermediateDirectories: true)
    } else {
      try? fm.removeItem(at: destURL)
      try? fm.copyItem(at: itemURL, to: destURL)
    }
  }
}

// MARK: - Source file collection
// Mirrors Dir.glob("ios/**/*.{h,hpp,m,mm}") + Dir.glob("cpp/**/*.{hpp,h,cpp,c}")

func relativePath(_ url: URL, from base: URL) -> String {
  let baseComponents = base.standardizedFileURL.pathComponents
  let urlComponents = url.standardizedFileURL.pathComponents
  return urlComponents.dropFirst(baseComponents.count).joined(separator: "/")
}

func globFiles(under subdirectory: String, extensions: Set<String>) -> [String] {
  let dir = packageRoot.appendingPathComponent(subdirectory)
  guard let enumerator = FileManager.default.enumerator(at: dir, includingPropertiesForKeys: [.isDirectoryKey]) else {
    return []
  }
  var results: [String] = []
  for case let fileURL as URL in enumerator {
    let isDirectory = (try? fileURL.resourceValues(forKeys: [.isDirectoryKey]))?.isDirectory ?? false
    if isDirectory { continue }
    if extensions.contains(fileURL.pathExtension.lowercased()) {
      results.append(relativePath(fileURL, from: packageRoot))
    }
  }
  return results.sorted()
}

var sourceFiles =
  globFiles(under: "ios", extensions: ["h", "hpp", "m", "mm"])
  + globFiles(under: "cpp", extensions: ["hpp", "h", "cpp", "c"])

// Every real file SPM finds under the target's `path` that isn't excluded
// still gets swept into the build as a bogus "compile" step — confirmed
// empirically on this toolchain (Xcode 26.6 / swift-tools 6.0) for
// individual files living alongside wanted siblings (a disabled backend's
// lone .cpp file, a stray LICENSE.txt). Whole, self-contained directory
// excludes (the vendored xcframeworks, and cpp/sqlcipher, cpp/libsql,
// cpp/turso below) are reliable; those backend sources were relocated into
// their own subdirectories specifically so they can be excluded that way.
// `cpp/sqlite3.c`/`cpp/bridge.{c,cpp,hpp}` are the remaining individual-file
// exclusions (only reachable via the sqlcipher/turso/libsql/phone-version
// branches, not the default path) — flagged as a known residual risk in
// those configurations rather than moved, to keep this translation close to
// the podspec's file layout.
var excludedSources: Set<String> = []
var excludedDirs: Set<String> = []

// Backend bridges are selected explicitly by flags and should not be compiled by default.
if !useTurso {
  excludedDirs.insert("cpp/turso")
}

// MARK: - Tokenizers
// Set to non-user-app value ("../example/c_sources/tokenizers.h") by default;
// swapped to "../c_sources/tokenizers.h" below when installed as a dependency.
// Both branches faithfully mirror op-sqlite.podspec, which points at the
// *original* c_sources dir in dev mode but the *copy* (below) once installed.
var tokenizersHeaderPath = "../example/c_sources/tokenizers.h"

if !tokenizers.isEmpty {
  print("[OP-SQLITE] Tokenizers enabled: \(tokenizers)")

  let cSourcesDir =
    isUserApp
    ? packageRoot.deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
      .appendingPathComponent("c_sources")
    : packageRoot.appendingPathComponent("example/c_sources")
  let localCSourcesDir = packageRoot.appendingPathComponent("c_sources")

  generateTokenizersHeaderFile(
    names: tokenizers, filePath: cSourcesDir.appendingPathComponent("tokenizers.h").path)
  copyDirectoryContents(from: cSourcesDir, to: localCSourcesDir)

  sourceFiles += globFiles(under: "c_sources", extensions: ["h", "cpp"])
  tokenizersHeaderPath = isUserApp ? "../c_sources/tokenizers.h" : "../example/c_sources/tokenizers.h"
}

// MARK: - Backend selection (exclude_files equivalent)

if useSqlcipher {
  print("[OP-SQLITE] using SQLCipher")
  excludedSources.formUnion(["cpp/sqlite3.c", "cpp/sqlite3.h"])
  excludedDirs.insert("cpp/libsql")
} else if useTurso {
  print("[OP-SQLITE] using Turso SDK kit")
  excludedSources.formUnion(["cpp/sqlite3.c", "cpp/sqlite3.h", "cpp/bridge.hpp", "cpp/bridge.cpp"])
  excludedDirs.formUnion(["cpp/sqlcipher", "cpp/libsql"])
} else if useLibsql {
  print("[OP-SQLITE] ⚠️ Using libsql. If you have libsql questions please ask in the Turso Discord server.")
  // cpp/bridge.cpp is the default (non-libsql) bridge; it and
  // cpp/libsql/bridge.cpp both define opsqlite_get_db_path and friends —
  // compiling both is a duplicate-symbol link error.
  excludedSources.formUnion(["cpp/sqlite3.c", "cpp/sqlite3.h", "cpp/bridge.cpp", "cpp/bridge.hpp"])
  excludedDirs.insert("cpp/sqlcipher")
} else {
  print("[OP-SQLITE] using pure SQLite")
  excludedDirs.formUnion(["cpp/sqlcipher", "cpp/libsql"])
}

if phoneVersion {
  print("[OP-SQLITE] using iOS embedded SQLite 📱")
  excludedSources.formUnion(["cpp/sqlite3.c", "cpp/sqlite3.h"])
}

// A prior manifest evaluation (tokenizers previously enabled) may have left
// c_sources/*.{h,cpp} on disk even though tokenizers is empty now — exclude
// the whole dir so a stale copy doesn't get swept into the build.
if tokenizers.isEmpty {
  excludedDirs.insert("c_sources")
}

sourceFiles.removeAll { excludedSources.contains($0) }
sourceFiles = Array(Set(sourceFiles)).sorted()

// MARK: - Preprocessor defines (GCC_PREPROCESSOR_DEFINITIONS + OTHER_CFLAGS)

func parseDefines(_ flags: String) -> [(name: String, value: String?)] {
  flags.split(separator: " ").compactMap { token in
    guard token.hasPrefix("-D") else { return nil }
    let body = token.dropFirst(2)
    if let eq = body.firstIndex(of: "=") {
      return (String(body[body.startIndex..<eq]), String(body[body.index(after: eq)...]))
    }
    return (String(body), nil)
  }
}

var defines: [(name: String, value: String?)] = parseDefines(
  "-DSQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION=1 -DHAVE_USLEEP=1 -DSQLITE_ENABLE_LOCKING_STYLE=0")

if useSqlcipher {
  defines += [
    ("OP_SQLITE_USE_SQLCIPHER", "1"),
    ("HAVE_FULLFSYNC", "1"),
    ("SQLITE_HAS_CODEC", nil),
    ("SQLITE_TEMP_STORE", "3"),
    ("SQLITE_EXTRA_INIT", "sqlcipher_extra_init"),
    ("SQLITE_EXTRA_SHUTDOWN", "sqlcipher_extra_shutdown"),
  ]
}
if fts5 { defines.append(("SQLITE_ENABLE_FTS5", "1")) }
if rtree { defines.append(("SQLITE_ENABLE_RTREE", "1")) }
if phoneVersion { defines.append(("OP_SQLITE_USE_PHONE_VERSION", "1")) }
if performanceMode {
  print("[OP-SQLITE] Performance mode enabled")
  defines += parseDefines(
    "-DSQLITE_DQS=0 -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1 -DSQLITE_LIKE_DOESNT_MATCH_BLOBS=1 -DSQLITE_MAX_EXPR_DEPTH=0 -DSQLITE_OMIT_DEPRECATED=1 -DSQLITE_OMIT_PROGRESS_CALLBACK=1 -DSQLITE_OMIT_SHARED_CACHE=1 -DSQLITE_USE_ALLOCA=1 -DSQLITE_STRICT_SUBTYPE=1 -DSQLITE_THREADSAFE=2"
  )
}
if useCrsqlite {
  print("[OP-SQLITE] using CRQSQLite 🤖")
  defines.append(("OP_SQLITE_USE_CRSQLITE", "1"))
}
if useSqliteVec {
  print("[OP-SQLITE] using Sqlite Vec ↗️")
  defines.append(("OP_SQLITE_USE_SQLITE_VEC", "1"))
}
if useLibsql {
  defines.append(("OP_SQLITE_USE_LIBSQL", "1"))
}
if useTurso {
  defines.append(("OP_SQLITE_USE_TURSO", "1"))
}
if !sqliteFlags.isEmpty {
  print("[OP-SQLITE] Detected custom SQLite flags: \(sqliteFlags)")
  defines += parseDefines(sqliteFlags)
}
if !tokenizers.isEmpty {
  defines.append(("TOKENIZERS_HEADER_PATH", "\\\"\(tokenizersHeaderPath)\\\""))
}

let defineCSettings: [CSetting] = defines.map { name, value in
  value != nil ? .define(name, to: value!) : .define(name)
}
let defineCXXSettings: [CXXSetting] = defines.map { name, value in
  value != nil ? .define(name, to: value!) : .define(name)
}

// MARK: - Vendored xcframeworks

var targetDependencies: [Target.Dependency] = []
var binaryTargets: [Target] = []

if useCrsqlite {
  binaryTargets.append(.binaryTarget(name: "OpSqliteCRSQLite", path: "ios/crsqlite.xcframework"))
  targetDependencies.append(.target(name: "OpSqliteCRSQLite"))
}
if useSqliteVec {
  binaryTargets.append(.binaryTarget(name: "OpSqliteSqliteVec", path: "ios/sqlitevec.xcframework"))
  targetDependencies.append(.target(name: "OpSqliteSqliteVec"))
}
if useLibsql {
  binaryTargets.append(
    .binaryTarget(name: "OpSqliteLibsqlExperimental", path: "ios/libsql_experimental.xcframework"))
  targetDependencies.append(.target(name: "OpSqliteLibsqlExperimental"))
}
if useTurso {
  binaryTargets.append(.binaryTarget(name: "OpSqliteTursoSdkKit", path: "ios/turso_sdk_kit.xcframework"))
  targetDependencies.append(.target(name: "OpSqliteTursoSdkKit"))
}

// MARK: - Linker settings (s.library "sqlite3" for the phone/embedded-SQLite variant)

var linkerSettings: [LinkerSetting] = []
if phoneVersion {
  linkerSettings.append(.linkedLibrary("sqlite3"))
}

// MARK: - React Native + package dependencies
// Per the hand-authored community library contract (RN's
// spm-header-paths-contract.md): a self-managed dep is symlinked by the
// autolinker at a fixed depth, `<appRoot>/build/generated/autolinking/libs/<SwiftName>/`,
// so these relative paths to the app-local ReactNative / React-GeneratedCode
// packages are stable — the same ones RN's own scaffolder emits.

var packageDependencies: [Package.Dependency] = [
  .package(name: "ReactNative", path: "../../../../xcframeworks"),
  .package(name: "React-GeneratedCode", path: "../../../ios"),
]
targetDependencies += [
  .product(name: "ReactHeaders", package: "ReactNative"),
  .product(name: "ReactNativeHeaders", package: "ReactNative"),
  .product(name: "ReactNativeDependenciesHeaders", package: "ReactNative"),
  .product(name: "ReactAppHeaders", package: "React-GeneratedCode"),
]

// Best-effort translation of `s.dependency "OpenSSL-Universal"`. There's no
// SwiftPM release of the CocoaPods OpenSSL-Universal binary; this points at
// the closest maintained SwiftPM OpenSSL package instead. The example app
// doesn't enable sqlcipher, so this path is untested — verify ABI/symbol
// compatibility before shipping a sqlcipher-enabled SPM build.
if useSqlcipher {
  packageDependencies.append(
    .package(url: "https://github.com/krzyzanowskim/OpenSSL.git", from: "3.3.3000"))
  targetDependencies.append(.product(name: "OpenSSL", package: "OpenSSL"))
}

// MARK: - Package

// `path: "."` puts the whole repo root in scope for SPM's own directory
// walk. `example/` contains the autolinker's self-managed-package symlink
// for this very package (`example/ios/build/generated/autolinking/libs/OpSqlite`
// -> this repo root), so an unbounded walk recurses through that symlink
// forever. Bound the walk to only the top-level dirs the target needs.
//
// NOTE: SwiftPM's manifest evaluation runs in a filesystem sandbox that
// forbids writes outside temp/build directories (confirmed empirically: an
// attempt to stage files into a repo-local scratch dir failed with EPERM,
// even though the tokenizer-header write above degrades gracefully via
// `try?`). That rules out physically relocating unwanted files at resolve
// time — the fix has to be purely declarative `exclude:` entries instead,
// which is why the backend-specific sources above are excluded by directory
// wherever the repo layout allows it.
let neededTopLevelDirs: Set<String> = ["ios", "cpp", "c_sources"]
let topLevelExcludes: [String] =
  (try? FileManager.default.contentsOfDirectory(atPath: packageRoot.path))?
  .filter { !neededTopLevelDirs.contains($0) && $0 != "Package.swift" && $0 != "op-sqlite-spm-prefix.h" }
  ?? []

// Vendored xcframeworks are consumed exclusively through their own
// `.binaryTarget` declarations below — excluded here unconditionally so the
// main target's directory scan never sweeps their Info.plist / CodeResources
// / raw Mach-O binaries in as bogus "sources".
let targetExcludes =
  topLevelExcludes
  + [
    "ios/crsqlite.xcframework",
    "ios/sqlitevec.xcframework",
    "ios/libsql_experimental.xcframework",
    "ios/turso_sdk_kit.xcframework",
    "ios/OPSQLite.xcodeproj",
  ]
  + excludedDirs.map { $0 }
  + excludedSources.map { $0 }

let package = Package(
  name: "OpSqlite",
  // RN's own SPM packaging (generate-spm-autolinking.js) only declares
  // .iOS(.v15) for the autolinked aggregator; a broader platform list here
  // (tvOS/macOS/visionOS, mirroring op-sqlite.podspec) makes SPM reject the
  // whole graph with a minimum-deployment-target conflict, since the
  // aggregator doesn't guarantee those platforms. RN's SwiftPM support is
  // iOS-only at this stage regardless.
  platforms: [.iOS(.v15)],
  products: [
    .library(name: "OpSqlite", targets: ["OpSqlite"])
  ],
  dependencies: packageDependencies,
  targets: binaryTargets + [
    .target(
      name: "OpSqlite",
      dependencies: targetDependencies,
      path: ".",
      // Deliberately no `sources:` alongside `exclude:` — combining an
      // explicit `sources:` allowlist with `exclude:` proved unreliable on
      // this toolchain (files covered by `exclude:` but absent from
      // `sources:` still got swept into the build). `targetExcludes` is
      // constructed so everything left under `path` is exactly the wanted
      // set, and the default auto-scan is the well-trodden SPM code path.
      exclude: targetExcludes,
      // cpp/bridge.cpp reaches SQLite via `#include <sqlite3.h>` (angle
      // form). cpp/sqlite3.h and cpp/sqlcipher/sqlite3.h are two different
      // files with that same name; CocoaPods disambiguates via its header
      // map, built fresh from source_files/exclude_files each `pod install`.
      // SPM has no header map — publicHeadersPath is the closest equivalent
      // (SPM exposes/searches that one directory's headers for the target),
      // so it has to track the same backend switch as the exclude logic
      // above, or SQLCipher's build silently sees plain SQLite's sqlite3.h
      // (no sqlite3_key_v2) instead of its own.
      publicHeadersPath: useSqlcipher ? "cpp/sqlcipher" : "cpp",
      cSettings: defineCSettings + [
        .headerSearchPath("."),
        // Explicit backstop for the same sqlite3.h disambiguation described
        // above, in case publicHeadersPath's implicit search isn't given
        // priority over some other path on a future toolchain.
        .headerSearchPath(useSqlcipher ? "cpp/sqlcipher" : "cpp"),
        // cpp/libsql/bridge.hpp unconditionally `#include`s libsql.h (it's
        // pulled in from always-compiled files like DBHostObject.hpp, not
        // just the libsql backend), so libsql.h must resolve regardless of
        // whether useLibsql links the actual xcframework binary. CocoaPods
        // papered over this with a broad recursive header search path;
        // SPM has no such default, so it's added explicitly here. Excluding
        // the whole `cpp/libsql` dir above (when !useLibsql) only removes it
        // from SPM's own source bookkeeping — the header stays on disk and
        // still resolves via this search path regardless.
        .headerSearchPath("ios/libsql_experimental.xcframework/ios-arm64/libsql_experimental.framework/Headers"),
        .unsafeFlags(
          ["-O2", "-include", "op-sqlite-spm-prefix.h"]
            // cpp/turso/turso_bridge.cpp includes <turso_sdk_kit/turso.h> —
            // framework-style angle include. The xcframework ships no
            // module map, so this only resolves via clang's plain
            // framework-header lookup, which needs an explicit -F pointing
            // at the directory containing turso_sdk_kit.framework (headers
            // are identical across arch slices, so any one slice works).
            + (useTurso ? ["-F", "ios/turso_sdk_kit.xcframework/ios-arm64"] : [])),
      ],
      cxxSettings: defineCXXSettings + [
        .headerSearchPath("."),
        .headerSearchPath(useSqlcipher ? "cpp/sqlcipher" : "cpp"),
        .headerSearchPath("ios/libsql_experimental.xcframework/ios-arm64/libsql_experimental.framework/Headers"),
        .unsafeFlags(
          ["-O2", "-include", "op-sqlite-spm-prefix.h"]
            + (useTurso ? ["-F", "ios/turso_sdk_kit.xcframework/ios-arm64"] : [])),
      ],
      linkerSettings: linkerSettings
    )
  ],
  cxxLanguageStandard: .cxx2b
)
