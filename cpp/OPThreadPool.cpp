#include "OPThreadPool.hpp"

#include <algorithm>

namespace opsqlite {

ThreadPool::ThreadPool() {
  // This returns the number of threads supported by the system. If the
  // function can't figure out this information, it returns 0. 0 is not good,
  // so we create at least 1
  //  auto number_of_threads = std::thread::hardware_concurrency();
  //  if (number_of_threads == 0) {
  //    number_of_threads = 1;
  //  }

  // To keep sqlite threading model working, we cannot access via multiple
  // threads in the same process therefore we cap the number of off-threads
  // to one per connection
  //
  // You can achieve more performance when using more threads but you run
  // into race conditions. This request was brought forth by PowerSync.
  auto number_of_threads = 1;
  try {
    for (unsigned i = 0; i < number_of_threads; ++i) {
      // The threads will execute the private member `do_work`. Note that we
      // need to pass a reference to the function (namespaced with the class
      // name) as the first argument, and the current object as second
      // argument
      threads.emplace_back(&ThreadPool::do_work, this);
      worker_ids.push_back(threads.back().get_id());
    }
  } catch (...) {
    // The destructor does not run when a constructor throws, and destroying a
    // joinable std::thread calls std::terminate. Stop and join the threads
    // that did start before letting the exception out.
    stop_accepting_work();
    join_threads();
    throw;
  }
}

// Joins the threads so the process (or in our RN case, this
// main-thread/runtime generation) can exit gracefully. A no-op when shutdown()
// already ran. Never runs on a worker as long as no task owns the pool (see the
// rule in the header).
ThreadPool::~ThreadPool() { shutdown(); }

bool ThreadPool::is_worker_thread() const {
  return std::find(worker_ids.begin(), worker_ids.end(),
                   std::this_thread::get_id()) != worker_ids.end();
}

void ThreadPool::stop_accepting_work() {
  {
    // Set under the mutex so a worker between its predicate check and its
    // wait cannot miss the wake-up below.
    std::lock_guard<std::mutex> g(work_queue_mutex);
    done = true;
  }

  // Wake up all the threads, so they can drain the queue and finish
  work_pending.notify_all();
}

void ThreadPool::join_threads() {
  // A second caller blocks here until the first caller's join has completed,
  // so it can never free this object while the worker is still running.
  std::lock_guard<std::mutex> g(join_mutex);
  for (auto &thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  threads.clear();
}

void ThreadPool::shutdown() {
  stop_accepting_work();

  if (is_worker_thread()) {
    // Called from a task (e.g. a database invalidated on the worker). The
    // worker cannot join itself, and it will not pick up another task until
    // this one returns, so run what is left here: when shutdown() returns,
    // nothing else will run, as on every other thread. `busy` still counts the
    // calling task, which do_work() releases as usual.
    while (true) {
      std::function<void(void)> task;
      {
        std::lock_guard<std::mutex> g(work_queue_mutex);
        if (work_queue.empty()) {
          break;
        }
        task = std::move(work_queue.front());
        work_queue.pop();
      }
      task();
    }
    work_pending.notify_all();
    return;
  }

  join_threads();
}

// This function will be called by the server every time there is a request
// that needs to be processed by the thread pool
void ThreadPool::queue_work(const std::function<void(void)> &task) {
  // Grab the mutex
  std::lock_guard<std::mutex> g(work_queue_mutex);

  // A stopped pool has no worker left to run the task, and its promise would
  // never settle. Callers map this to their own "database is closed" error.
  if (done) {
    throw ThreadPoolStoppedError();
  }

  // Push the request to the queue
  work_queue.push(task);

  // Wake every waiter. wait_finished() and do_work() share this condition
  // so we force all conditions to check
  work_pending.notify_all();
}

// Function used by the threads to grab work from the queue
void ThreadPool::do_work() {
  while (true) {
    std::function<void(void)> task;

    // Create a scope, so we don't lock the queue for longer than necessary
    {
      std::unique_lock<std::mutex> g(work_queue_mutex);
      work_pending.wait(g, [&] {
        // Only wake up if there are elements in the queue or the
        // program is shutting down
        return done || !work_queue.empty();
      });

      // Shutting down: finish only once every queued task has run, so no
      // promise is left unsettled and wait_finished() cannot block forever.
      if (work_queue.empty()) {
        break;
      }

      task = work_queue.front();
      work_queue.pop();
      ++busy;
    }

    task();

    // Release the task (and everything it captured, e.g. JSI values) before
    // signalling idle, so wait_finished()/close() can't observe busy == 0
    // while task-owned resources are still pending destruction.
    task = nullptr;

    {
      std::lock_guard<std::mutex> g(work_queue_mutex);
      --busy;
    }
    work_pending.notify_all();
  }
}

void ThreadPool::wait_finished() {
  std::unique_lock<std::mutex> g(work_queue_mutex);
  work_pending.wait(g, [&] { return work_queue.empty() && (busy == 0); });
}

} // namespace opsqlite
