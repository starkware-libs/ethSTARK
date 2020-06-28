#ifndef STARKWARE_UTILS_TASK_MANAGER_H_
#define STARKWARE_UTILS_TASK_MANAGER_H_

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "gflags/gflags.h"
#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/error_handling/error_handling.h"

DECLARE_uint32(n_threads);

namespace starkware {

struct TaskInfo {
  uint64_t start_idx;
  uint64_t end_idx;
};

/*
  This class manages task executions.

  It maintains a queue of pending tasks to execute, and a thread pool with (n_threads - 1)
  execution threads.

  When Execute(new_tasks) is called, the new_tasks are added to the queue and
  the calling thread joins the thread pool until all the new_tasks are completed.

  This design makes it easier to support hierarchical parallelization, as tasks
  may invoke QueueAndWait without reducing the number of threads allocated for the task execution.
*/
class TaskManager {
 public:
  TaskManager(const TaskManager&) = delete;
  TaskManager(TaskManager&&) = delete;
  TaskManager& operator=(const TaskManager&) = delete;
  TaskManager& operator=(TaskManager&&) = delete;

  ~TaskManager();

  /*
    Executes func on each item within [begin, end).
    Returns when all tasks are completed.

    If an exception occurs during the execution of func it is captured and re-thrown
    from the thread that invoked ParallelFor.

    max_chunk_size_for_lambda limits chunks size passed to the lambda function.
    max_chunk_size_for_lambda = 1, implies tasks of size 1 (end_idx = start_idx + 1).

    min_work_chunk controls how fine grained the parallelization, if the number of tasks
    is smaller than min_work_chunk, then all the tasks will be executed by a single thread.

    Note that the following configuration is valid:
      a.
          start_idx = 0, end_idx = 20
          max_chunk_size_for_lambda = 10;
          min_work_chunk == 1
        And the implementation may call the lambda twice with the following ranges:
          [1, 10),
          [10, 20).

      b.
          start_idx = 0, end_idx = 20
          max_chunk_size_for_lambda = 80;
          min_work_chunk == 40
         And the implementation will call the lambda once with the range
          [1, 20].
  */
  void ParallelFor(
      uint64_t start_idx, uint64_t end_idx, const std::function<void(const TaskInfo&)>& func,
      uint64_t max_chunk_size_for_lambda = 1, uint64_t min_work_chunk = 1);

  void ParallelFor(
      uint64_t end_idx, const std::function<void(const TaskInfo&)>& func,
      uint64_t max_chunk_size_for_lambda = 1, uint64_t min_work_chunk = 1) {
    ParallelFor(0U, end_idx, func, max_chunk_size_for_lambda, min_work_chunk);
  }

  static TaskManager& GetInstance() {
    std::call_once(singleton_flag, InitSingleton);
    return *singleton;
  }

  /*
    Returns the number of threads used to execute tasks.
    Since we also use the main thread for execution, this is the number of worker threads + 1.
  */
  size_t GetNumThreads() { return workers_.size() + 1; }

  /*
    For tests settings only.
    An interface to circumvent the singleton pattern.
    Used in tests where we want to test different thread number settings.
  */
  static TaskManager CreateInstanceForTesting(
      size_t n_threads = std::thread::hardware_concurrency());

  /*
    Returns the worker_id of the current thread.
  */
  static size_t GetWorkerId() {
    return worker_id;  // Read worker_id from thread local storage.
  }

 private:
  /*
    Sets the worker_id of the current thread.
  */
  static void SetWorkerIdForCurrentThread(size_t id) {
    worker_id = id;  // Save worker_id to thread local storage.
  }

  /*
    When adding tasks to a queue, we try to create kTaskRedundancyFactor*GetNumThreads() task.
    this paramater is a tradeoff between the desire to minimize the number of tasks and
    tail latency due to unbalanced execution speed.
  */
  static constexpr uint64_t kTaskRedundancyFactor = 4;

  explicit TaskManager(size_t n_threads);
  static void InitSingleton();

  class CvWithWaitersCount {
   public:
    /*
      This function should be called with the lock held, just like a normal condition variable.
    */
    void Wait(std::unique_lock<std::mutex>* lock);
    bool TryNotify();
    void NotifyAll();

   private:
    std::condition_variable cv_;
    size_t n_sleeping_threads_ = 0;
  };

  /*
    Run tasks from the tasks queue (tasks_) until the siblings_counter is reduced to 0.
    When the worker threads (workers_) are created they start executing WorkerTask
    with siblings_counter = &continue_running_.
    continue_running_ is set to 0 only in the destructor.

    When a thread calls ParallelFor (or Execute) it adds a group of "sibling" tasks
    sharing the same siblings_counter to tasks_, and calls TaskManager(&siblings_counter)
    to execute tasks until all the sibling tasks finish.
    Each sibling tasks decreases the sibling count when it is done.
  */
  void TaskRunner(CvWithWaitersCount* cv, const size_t* siblings_counter);

  std::mutex mutex_;

  std::vector<std::thread> workers_;
  std::vector<std::function<void()>> tasks_;

  CvWithWaitersCount new_pending_task_;
  CvWithWaitersCount task_group_finished_;

  size_t continue_running_ = 1;

  static gsl::owner<TaskManager*> singleton;
  static std::once_flag singleton_flag;
  static thread_local size_t worker_id;
};

}  // namespace starkware

#include "starkware/utils/task_manager.inl"

#endif  // STARKWARE_UTILS_TASK_MANAGER_H_
