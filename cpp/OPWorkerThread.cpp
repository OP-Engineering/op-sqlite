#include "OPWorkerThread.hpp"

namespace opsqlite {

WorkerThread::WorkerThread() : thread(&WorkerThread::do_work, this) {
  thread_id = thread.get_id();
}

// Joins the thread so the process (or in our RN case, this
// main-thread/runtime generation) can exit gracefully. A no-op when shutdown()
// already ran.
WorkerThread::~WorkerThread() { shutdown(); }

void WorkerThread::shutdown() {
  // Checked before call_once: on the worker thread it could otherwise block on
  // another caller's join of this very thread.
  if (std::this_thread::get_id() == thread_id) {
    throw std::system_error(
        std::make_error_code(std::errc::resource_deadlock_would_occur),
        "[op-sqlite] WorkerThread::shutdown() called from its own thread");
  }

  std::call_once(shutdown_once, [this] {
    {
      // Set under the mutex so the thread, between its predicate check and
      // its wait, cannot miss the wake-up below.
      std::lock_guard<std::mutex> g(mutex);
      done = true;
    }
    work_pending.notify_one();
    thread.join();
  });
}

// Called by the database and prepared statement host objects (through
// promisify and the reactive flush) for every async call
void WorkerThread::queue_work(const std::function<void(void)> &task) {
  {
    std::lock_guard<std::mutex> g(mutex);

    // A stopped worker has no thread left to run the task, and its promise
    // would never settle. Callers map this to their own "database is closed"
    // error.
    if (done) {
      throw WorkerThreadStoppedError();
    }

    work_queue.push(task);
  }

  work_pending.notify_one();
}

void WorkerThread::do_work() {
  while (true) {
    std::function<void(void)> task;

    // Create a scope, so we don't lock the queue for longer than necessary
    {
      std::unique_lock<std::mutex> g(mutex);
      work_pending.wait(g, [&] { return done || !work_queue.empty(); });

      // Shutting down: finish only once every queued task has run, so no
      // promise is left unsettled.
      if (work_queue.empty()) {
        return;
      }

      task = std::move(work_queue.front());
      work_queue.pop();
    }

    // The task (and everything it captured, e.g. JSI values) is released at
    // the end of this iteration, before the thread can exit and be joined.
    task();
  }
}

} // namespace opsqlite
