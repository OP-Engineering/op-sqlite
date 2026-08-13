#pragma once

#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <queue>
#include <stdio.h>
#include <thread>
#include <vector>

namespace opsqlite {

class ThreadPool {
public:
  ThreadPool();
  ~ThreadPool();
  void queue_work(const std::function<void(void)> &task);
  void wait_finished();

private:
  unsigned int busy{};
  // This condition variable is used for the threads to wait until there is
  // work to do
  std::condition_variable_any work_pending;

  // We store the threads in a vector, so we can later stop them gracefully
  std::vector<std::thread> threads;

  // Mutex to protect work_queue
  std::mutex work_queue_mutex;

  // Queue of requests waiting to be processed
  std::queue<std::function<void(void)>> work_queue;

  // This will be set to true when the thread pool is shutting down. This
  // tells the threads to stop looping and finish.
  // Atomic because do_work() reads it in `while (!done)` outside the mutex.
  std::atomic<bool> done;

  // Function used by the threads to grab work from the queue
  void do_work();
};

} // namespace opsqlite