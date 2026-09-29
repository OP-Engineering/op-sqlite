/**
 * Rolls back the open transaction after `error` and rethrows `error`.
 *
 * SQLite sometimes rolls a transaction back on its own before we get to it:
 * RAISE(ROLLBACK) in a trigger, or a COMMIT failing with SQLITE_FULL,
 * SQLITE_IOERR* or SQLITE_CORRUPT_VTAB. A ROLLBACK issued after that fails with
 * "cannot rollback - no transaction is active", and that failure must never
 * replace the error that caused it.
 *
 * `inTransaction` is undefined on backends that cannot report the transaction
 * state (libsql, web). There the ROLLBACK is always attempted and a failure is
 * attached to the original error as `rollbackError`.
 */
export async function rollbackAndRethrow(
  error: unknown,
  rollback: () => unknown,
  inTransaction?: () => boolean,
): Promise<never> {
  if (inTransaction?.() === false) {
    throw error;
  }

  try {
    await rollback();
  } catch (rollbackError) {
    throw withRollbackError(error, rollbackError, inTransaction);
  }

  throw error;
}

function withRollbackError(
  error: unknown,
  rollbackError: unknown,
  inTransaction?: () => boolean,
): unknown {
  // The connection is stuck inside the failed transaction: every following
  // statement would silently run in it and be lost. That matters more than the
  // original error, which is kept as `cause`.
  if (inTransaction?.() === true) {
    const stuck = new Error(
      `[op-sqlite] ROLLBACK failed and the connection is still inside a transaction: ${messageOf(rollbackError)}`,
    ) as Error & { cause?: unknown; rollbackError?: unknown };
    stuck.cause = error;
    stuck.rollbackError = rollbackError;
    return stuck;
  }

  if (error !== null && typeof error === "object") {
    try {
      (error as { rollbackError?: unknown }).rollbackError = rollbackError;
    } catch {
      // Frozen or otherwise non-extensible error, rethrow it untouched
    }
  }

  return error;
}

function messageOf(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}
