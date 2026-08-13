#include "OPThreadPool.hpp"

namespace opsqlite {

ThreadPool::ThreadPool() : done(false) {
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
  for (unsigned i = 0; i < number_of_threads; ++i) {
    // The threads will execute the private member `do_work`. Note that we
    // need to pass a reference to the function (namespaced with the class
    // name) as the first argument, and the current object as second
    // argument
    threads.emplace_back(&ThreadPool::do_work, this);
  }
}

// The destructor joins all the threads so the process (or in our RN case, this
// main-thread/runtime generation) can exit gracefully. This will be executed if
// there is any exception (e.g. creating the threads)
ThreadPool::~ThreadPool() {
  // So threads know it's time to shut down
  done = true;

  // Wake up all the threads, so they can finish and be joined
  work_pending.notify_all();

  for (auto &thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  threads.clear();
}

// This function will be called by the server every time there is a request
// that needs to be processed by the thread pool
void ThreadPool::queue_work(const std::function<void(void)> &task) {
  // Grab the mutex
  std::lock_guard<std::mutex> g(work_queue_mutex);

  // Push the request to the queue
  work_queue.push(task);

  // Wake every waiter. wait_finished() and do_work() share this condition
  // so we force all conditions to check
  work_pending.notify_all();
}

// Function used by the threads to grab work from the queue
void ThreadPool::do_work() {
  // Loop while the queue is not destructing
  while (!done) {
    std::function<void(void)> task;

    // Create a scope, so we don't lock the queue for longer than necessary
    {
      std::unique_lock<std::mutex> g(work_queue_mutex);
      work_pending.wait(g, [&] {
        // Only wake up if there are elements in the queue or the
        // program is shutting down
        return done || !work_queue.empty();
      });

      // If we are shutting down exit without trying to process more work
      if (done) {
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
