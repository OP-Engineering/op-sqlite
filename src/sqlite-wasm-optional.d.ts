declare module "@sqlite.org/sqlite-wasm" {
	export const sqlite3Worker1Promiser: any;
}

declare module "@sqlite.org/sqlite-wasm/sqlite3.wasm" {
	const url: string;
	export default url;
}
