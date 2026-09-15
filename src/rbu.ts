import type { RBUApplyOptions } from "./types";

const SQLITE_MISUSE = 21;

export class RBUError extends Error {
	code: number;

	constructor(code: number, message: string, cause?: unknown) {
		super(message);
		this.name = "RBUError";
		this.code = code;
		if (cause !== undefined) {
			(this as Error & { cause?: unknown }).cause = cause;
		}
	}
}

export function validateRBUOptions(options: RBUApplyOptions): void {
	if (options == null || typeof options !== "object") {
		throw new RBUError(
			SQLITE_MISUSE,
			"[op-sqlite][RBU] applyRBU expects an options object",
		);
	}
	if (
		typeof options.targetPath !== "string" ||
		!options.targetPath.startsWith("/")
	) {
		throw new RBUError(
			SQLITE_MISUSE,
			"[op-sqlite][RBU] targetPath must be an absolute path",
		);
	}
	if (
		typeof options.updatePath !== "string" ||
		!options.updatePath.startsWith("/")
	) {
		throw new RBUError(
			SQLITE_MISUSE,
			"[op-sqlite][RBU] updatePath must be an absolute path",
		);
	}
	if (
		options.statePath !== undefined &&
		(typeof options.statePath !== "string" ||
			!options.statePath.startsWith("/"))
	) {
		throw new RBUError(
			SQLITE_MISUSE,
			"[op-sqlite][RBU] statePath must be an absolute path",
		);
	}
	if (
		options.maxSteps !== undefined &&
		(!Number.isSafeInteger(options.maxSteps) || options.maxSteps < 1)
	) {
		throw new RBUError(
			SQLITE_MISUSE,
			"[op-sqlite][RBU] maxSteps must be a positive safe integer",
		);
	}
}

export function normalizeRBUError(error: unknown): RBUError {
	if (error instanceof RBUError) {
		return error;
	}

	const nativeError = error as { code?: unknown; message?: unknown };
	const code =
		typeof nativeError?.code === "number" ? nativeError.code : SQLITE_MISUSE;
	const message =
		typeof nativeError?.message === "string"
			? nativeError.message
			: "[op-sqlite][RBU] unknown RBU error";
	return new RBUError(code, message, error);
}
