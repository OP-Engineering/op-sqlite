// Isolated in its own module so bundlers that reject a bare `*.wasm` static
// import (e.g. Vite, which requires a `?url`/`?init` suffix) never have to
// parse this file at all -- it's only reached via a dynamic `import()` from
// opsqlite-web.worker.ts's Metro-only fallback path, which Vite never
// executes because its own `import.meta.url`-based resolution succeeds
// first. See opsqlite-web.worker.ts for the full explanation.
import wasmAssetUrl from "@sqlite.org/sqlite-wasm/sqlite3.wasm";

export default wasmAssetUrl;
