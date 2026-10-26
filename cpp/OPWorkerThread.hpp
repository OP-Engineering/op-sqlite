#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <system_error>
#include <thread>

namespace opsqlite {

// Thrown by queue_work() once the worker has been shut down. WorkerThread knows
// nothing about databases; callers map this to their own "database is closed"
// error.
class WorkerThreadStoppedError : public std::runtime_error {
public:
  WorkerThreadStoppedError()
      : std::runtime_error("[op-sqlite] worker thread is stopped") {}
};

// Runs a connection's async work, one task at a time, on a single background
// thread. To keep sqlite's threading model working a connection must not be
// used from several threads at once, so each connection gets exactly one
// (more threads were faster but raced; requested by PowerSync).
//
// Rule: a task must never own the WorkerThread it runs on, nor call
// shutdown(). The thread cannot join itself (shutdown() throws
// std::system_error). No task does today: the tasks queued by promisify and
// the reactive flush capture the database or the statement by raw pointer.
class WorkerThread {
public:
  WorkerThread();
  ~WorkerThread();
  // Throws WorkerThreadStoppedError after shutdown().
  void queue_work(const std::function<void(void)> &task);
  // Stops the worker: no new work is accepted, every task already queued still
  // runs (so no promise is left unsettled), then the thread is joined. Called
  // by close()/delete()/invalidate() rather than leaving it to the destructor,
  // because the WorkerThread is shared with prepared statements and pending
  // promises and may outlive the database until the JS object is garbage
  // collected.
  //
  // Idempotent and safe to call concurrently: a later caller blocks until the
  // first caller's join has completed.
  void shutdown();

private:
  // Protects work_queue and done
  std::mutex mutex;

  // Signalled when work is queued or the worker is shutting down
  std::condition_variable work_pending;

  // Tasks waiting to run, in order
  std::queue<std::function<void(void)>> work_queue;

  // Set by shutdown(). queue_work() then refuses new work, and the thread
  // exits once the queue is empty.
  bool done = false;

  std::once_flag shutdown_once;

  // Declared last so it starts only once everything above is constructed. If
  // it fails to start, nothing else needs cleaning up.
  std::thread thread;

  // Written only by the constructor, so shutdown() can read it without a lock
  std::thread::id thread_id;

  // The thread's loop: runs queued tasks until shut down and drained
  void do_work();
};

} // namespace opsqlite
