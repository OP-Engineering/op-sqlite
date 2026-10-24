// Standalone lifecycle checks for cpp/OPThreadPool.cpp. No React Native, no
// JSI: it compiles the pool on its own and drives it the way OPDatabase does.
// Run it under ThreadSanitizer and AddressSanitizer with
// scripts/test-thread-pool.sh.
//
// Every check runs under a watchdog: a hang (a wait_finished() that never
// returns, a join that never completes) is reported as a failure instead of
// stalling the run.
#include "OPThreadPool.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#ifdef __APPLE__
#include <mach/mach.h>
#else
#include <dirent.h>
#endif

using opsqlite::ThreadPool;
using opsqlite::ThreadPoolStoppedError;
using namespace std::chrono_literals;

static int thread_count() {
#ifdef __APPLE__
  thread_act_array_t list;
  mach_msg_type_number_t n = 0;
  if (task_threads(mach_task_self(), &list, &n) != KERN_SUCCESS) {
    return -1;
  }
  for (mach_msg_type_number_t i = 0; i < n; i++) {
    mach_port_deallocate(mach_task_self(), list[i]);
  }
  vm_deallocate(mach_task_self(), (vm_address_t)list, n * sizeof(thread_act_t));
  return (int)n;
#else
  int n = 0;
  if (DIR *dir = opendir("/proc/self/task")) {
    while (dirent *entry = readdir(dir)) {
      if (entry->d_name[0] != '.') {
        n++;
      }
    }
    closedir(dir);
  }
  return n;
#endif
}

static int failures = 0;

static void check(bool condition, const std::string &message) {
  std::printf("%s %s\n", condition ? "ok:  " : "FAIL:", message.c_str());
  std::fflush(stdout);
  if (!condition) {
    failures++;
  }
}

// Runs `body` on its own thread and gives up on the whole process if it does
// not finish in time: a hung thread cannot be recovered.
template <typename F>
static void with_watchdog(const char *name, std::chrono::milliseconds limit,
                          F body) {
  std::mutex m;
  std::condition_variable cv;
  bool finished = false;
  std::thread runner([&] {
    body();
    std::lock_guard<std::mutex> g(m);
    finished = true;
    cv.notify_all();
  });
  std::unique_lock<std::mutex> lock(m);
  if (!cv.wait_for(lock, limit, [&] { return finished; })) {
    std::printf("FAIL: %s did not finish within %lld ms (hang)\n", name,
                (long long)limit.count());
    std::printf("\nFAILED (hang)\n");
    std::fflush(stdout);
    std::_Exit(2);
  }
  lock.unlock();
  runner.join();
}

// A one-shot gate a task can block on, so a test controls exactly when the
// worker is busy.
struct Gate {
  std::mutex m;
  std::condition_variable cv;
  bool open = false;
  void release() {
    std::lock_guard<std::mutex> g(m);
    open = true;
    cv.notify_all();
  }
  void wait() {
    std::unique_lock<std::mutex> g(m);
    cv.wait(g, [&] { return open; });
  }
};

// (a) close() frees the worker now, while the pool is still referenced (by a
// prepared statement, or by the JS object awaiting GC).
static void closed_pools_release_their_threads() {
  const int base = thread_count();
  std::vector<std::shared_ptr<ThreadPool>> held;
  for (int i = 0; i < 300; i++) {
    auto pool = std::make_shared<ThreadPool>();
    std::atomic<bool> ran{false};
    pool->queue_work([&] { ran = true; });
    pool->shutdown();
    if (!ran) {
      check(false, "work queued before shutdown ran");
    }
    held.push_back(pool);
  }
  const int extra = thread_count() - base;
  check(extra <= 1, "300 shut down pools, still referenced, leave <= 1 extra "
                    "thread (extra=" +
                        std::to_string(extra) + ")");
}

// (b) Work queued on a stopped pool is refused with the pool's own error, so a
// promise rejects instead of waiting on a queue nobody drains.
static void queue_after_shutdown_throws() {
  auto pool = std::make_shared<ThreadPool>();
  pool->shutdown();
  bool threw = false;
  std::string what;
  try {
    pool->queue_work([] {});
  } catch (const ThreadPoolStoppedError &e) {
    threw = true;
    what = e.what();
  }
  check(threw, "queue_work after shutdown throws ThreadPoolStoppedError");
  check(what == "[op-sqlite] thread pool is stopped",
        "the error is the pool's generic one (\"" + what + "\")");
  pool->wait_finished();
  check(true, "wait_finished after shutdown returns");
}

// (c) Tasks queued before shutdown() all run before it returns, with or
// without a wait_finished() first, and on the destructor path too.
static void queued_tasks_drain_on_shutdown() {
  {
    auto pool = std::make_shared<ThreadPool>();
    Gate gate;
    std::atomic<int> ran{0};
    pool->queue_work([&] {
      gate.wait();
      ran++;
    });
    for (int i = 0; i < 50; i++) {
      pool->queue_work([&] { ran++; });
    }
    // The worker is parked in the first task, so all 50 are still queued
    // when shutdown() starts.
    std::thread opener([&] {
      std::this_thread::sleep_for(20ms);
      gate.release();
    });
    pool->shutdown();
    check(ran == 51, "shutdown() drains the queue before returning (ran=" +
                         std::to_string(ran.load()) + "/51)");
    pool->wait_finished();
    check(true, "wait_finished after a draining shutdown returns");
    opener.join();
  }
  {
    std::atomic<int> ran{0};
    Gate gate;
    std::thread opener;
    {
      auto pool = std::make_shared<ThreadPool>();
      pool->queue_work([&] {
        gate.wait();
        ran++;
      });
      for (int i = 0; i < 50; i++) {
        pool->queue_work([&] { ran++; });
      }
      opener = std::thread([&] {
        std::this_thread::sleep_for(20ms);
        gate.release();
      });
    } // ~ThreadPool, no shutdown() and no wait_finished() before it
    check(ran == 51, "the destructor drains the queue too (ran=" +
                         std::to_string(ran.load()) + "/51)");
    opener.join();
  }
}

// (d) Two concurrent shutdown() calls, then the destructor. Neither caller may
// return while the worker is still inside a task: the destructor that follows
// would free the mutex, condition variable and queue under it.
static void concurrent_shutdowns_then_destructor() {
  for (int round = 0; round < 20; round++) {
    auto *pool = new ThreadPool();
    Gate started;
    std::atomic<bool> task_finished{false};
    pool->queue_work([&] {
      started.release();
      std::this_thread::sleep_for(30ms);
      task_finished = true;
    });
    started.wait();

    std::atomic<bool> first_saw_finished{false};
    std::atomic<bool> second_saw_finished{false};
    std::thread first([&] {
      pool->shutdown();
      first_saw_finished = task_finished.load();
    });
    std::thread second([&] {
      // Start only once `first` has stopped the pool (queue_work refuses
      // work from then on) and is on its way into the join.
      while (true) {
        try {
          pool->queue_work([] {});
        } catch (const ThreadPoolStoppedError &) {
          break;
        }
        std::this_thread::sleep_for(1ms);
      }
      std::this_thread::sleep_for(5ms);
      pool->shutdown();
      second_saw_finished = task_finished.load();
      if (!second_saw_finished) {
        // Report before the delete below, which then frees the pool under
        // the running worker.
        check(false, "the second shutdown() returned before the join "
                     "completed");
      }
      delete pool; // ~ThreadPool right after a shutdown() returned
    });
    first.join();
    second.join();
    if (!first_saw_finished || !second_saw_finished) {
      check(false, "round " + std::to_string(round) +
                       ": a shutdown() returned while the worker was still "
                       "running a task");
      return;
    }
  }
  check(true, "concurrent shutdown() x2 + destructor: every caller waits for "
              "the join (20 rounds)");
}

// (e) shutdown() from a task on the pool's own worker, as OPDatabase's
// invalidate()/destructor would if it ran there. It must not self-join, it
// must not hang, the tasks behind it must still run, and `busy` must be
// released so wait_finished() returns.
static void shutdown_from_the_worker() {
  auto pool = std::make_shared<ThreadPool>();
  Gate queued;
  std::atomic<int> after{0};
  std::atomic<int> after_seen_by_shutdown{-1};
  std::atomic<bool> threw_on_worker{false};
  pool->queue_work([&] {
    queued.wait();
    pool->shutdown();
    // Runs the rest inline: nothing is left to run once shutdown() returns.
    after_seen_by_shutdown = after.load();
    try {
      pool->queue_work([] {});
    } catch (const ThreadPoolStoppedError &) {
      threw_on_worker = true;
    }
  });
  for (int i = 0; i < 10; i++) {
    pool->queue_work([&] { after++; });
  }
  queued.release();

  pool->wait_finished(); // hangs if do_work never decremented busy
  check(true, "wait_finished returns after shutdown() ran on the worker");
  check(after_seen_by_shutdown == 10,
        "shutdown() on the worker ran the queued tasks before returning "
        "(saw " +
            std::to_string(after_seen_by_shutdown.load()) + "/10)");
  check(threw_on_worker, "queue_work from the worker after its shutdown "
                         "throws");
  pool->wait_finished();
  check(true, "a second wait_finished also returns");
  pool->shutdown(); // joins the worker, which exited on its own
  check(true, "shutdown() from another thread then joins the worker");
  pool.reset();
  check(true, "destructor after a worker-side shutdown");
}

// (f) A constructor that cannot start its thread throws without terminating
// the process. The pool starts one thread, so this exercises the failure of
// the first (and only) one; the catch block's join of already-started
// threads matters only if number_of_threads is raised.
static void constructor_throw_is_clean() {
  std::mutex m;
  std::condition_variable cv;
  bool release = false;
  std::vector<std::thread> hogs;
  bool exhausted = false;
  for (int i = 0; i < 10000; i++) {
    try {
      hogs.emplace_back([&] {
        std::unique_lock<std::mutex> g(m);
        cv.wait(g, [&] { return release; });
      });
    } catch (const std::system_error &) {
      exhausted = true;
      break;
    }
  }
  bool threw = false;
  if (exhausted) {
    try {
      ThreadPool pool;
    } catch (const std::system_error &) {
      threw = true;
    }
  }
  {
    std::lock_guard<std::mutex> g(m);
    release = true;
  }
  cv.notify_all();
  for (auto &t : hogs) {
    t.join();
  }
  if (!exhausted) {
    check(true, "constructor throw: could not exhaust threads (" +
                    std::to_string(hogs.size()) + " started), skipped");
    return;
  }
  check(threw, "constructor throws std::system_error when its thread cannot "
               "start, without std::terminate (after " +
                   std::to_string(hogs.size()) + " threads)");
  // The pool still works once threads are available again.
  ThreadPool pool;
  std::atomic<bool> ran{false};
  pool.queue_work([&] { ran = true; });
  pool.shutdown();
  check(ran, "a pool constructed afterwards works");
}

int main() {
  with_watchdog("(a) closed pools release threads", 30s,
                closed_pools_release_their_threads);
  with_watchdog("(b) queue after shutdown", 10s, queue_after_shutdown_throws);
  with_watchdog("(c) drain on shutdown", 10s, queued_tasks_drain_on_shutdown);
  with_watchdog("(d) concurrent shutdowns + destructor", 30s,
                concurrent_shutdowns_then_destructor);
  with_watchdog("(e) shutdown from the worker", 10s, shutdown_from_the_worker);
  if (std::getenv("SKIP_CONSTRUCTOR_THROW") == nullptr) {
    with_watchdog("(f) constructor throw", 60s, constructor_throw_is_clean);
  }

  if (failures != 0) {
    std::printf("\n%d FAILED\n", failures);
    return 1;
  }
  std::printf("\nall passed\n");
  return 0;
}
