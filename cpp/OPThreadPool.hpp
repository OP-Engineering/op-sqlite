#pragma once

#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <stdio.h>
#include <thread>
#include <vector>

namespace opsqlite {

// Thrown by queue_work() once the pool has been shut down. The pool knows
// nothing about databases; callers map this to their own "database is closed"
// error.
class ThreadPoolStoppedError : public std::runtime_error {
public:
  ThreadPoolStoppedError()
      : std::runtime_error("[op-sqlite] thread pool is stopped") {}
};

// Rule: a task must never own the pool it runs on (capture a shared_ptr to
// it). If a task released the last reference, ~ThreadPool would run on the
// worker while do_work() still uses `this`. No task does today: the tasks
// queued by promisify and the reactive flush capture the database or the
// statement by raw pointer, never the pool.
class ThreadPool {
public:
  ThreadPool();
  ~ThreadPool();
  // Throws ThreadPoolStoppedError after shutdown().
  void queue_work(const std::function<void(void)> &task);
  void wait_finished();
  // Stops the pool: no new work is accepted, every task already queued still
  // runs (so no promise is left unsettled), then the worker is joined. When it
  // returns, no other task will run. Called by close()/delete()/invalidate()
  // rather than leaving it to the destructor, because the pool is shared with
  // prepared statements and pending promises and may outlive the database
  // until the JS object is garbage collected.
  //
  // Idempotent and safe to call concurrently: a later caller returns only
  // after the first caller's join has completed. Called from a task on the
  // worker itself, it runs the remaining queued tasks inline and returns
  // without joining (a thread cannot join itself). The worker then exits after
  // the current task, and a later shutdown() or the destructor joins it.
  void shutdown();

private:
  unsigned int busy{};
  // This condition variable is used for the threads to wait until there is
  // work to do
  std::condition_variable_any work_pending;

  // We store the threads in a vector, so we can later stop them gracefully.
  // Only touched by the constructor and, under join_mutex, by shutdown().
  std::vector<std::thread> threads;

  // Ids of the worker threads. Written only by the constructor, so shutdown()
  // can read it from any thread without a lock.
  std::vector<std::thread::id> worker_ids;

  // Mutex to protect work_queue
  std::mutex work_queue_mutex;

  // Held by shutdown() while it joins, so a second caller waits for that join
  // to finish instead of returning while the worker still uses this object.
  std::mutex join_mutex;

  // Queue of requests waiting to be processed
  std::queue<std::function<void(void)>> work_queue;

  // Set (under work_queue_mutex) by shutdown(). queue_work() then refuses new
  // work, and the threads stop once the queue is empty.
  bool done{false};

  bool is_worker_thread() const;
  void stop_accepting_work();
  void join_threads();

  // Function used by the threads to grab work from the queue
  void do_work();
};

} // namespace opsqlite
