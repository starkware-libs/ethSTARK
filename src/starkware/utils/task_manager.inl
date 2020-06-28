#include "starkware/utils/task_manager.h"

#include <type_traits>

namespace starkware {

inline void TaskManager::CvWithWaitersCount::Wait(std::unique_lock<std::mutex>* lock) {
  // Note that n_sleeping_threads_ is protected by lock.
  ++n_sleeping_threads_;
  cv_.wait(*lock);
  --n_sleeping_threads_;
}

inline bool TaskManager::CvWithWaitersCount::TryNotify() {
  bool has_waiter = n_sleeping_threads_ > 0;
  if (has_waiter) {
    cv_.notify_one();
  }
  return has_waiter;
}

inline void TaskManager::CvWithWaitersCount::NotifyAll() { cv_.notify_all(); }

}  // namespace starkware
