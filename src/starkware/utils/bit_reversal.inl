#include "starkware/utils/task_manager.h"

namespace starkware {

template <typename FieldElementT>
void BitReverseVector(gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst) {
  ASSERT_RELEASE(src.size() == dst.size(), "Span sizes of src and dst must be similar.");

  const int logn = SafeLog2(src.size());
  const size_t min_work_chunk = 1024;

  TaskManager& task_manager = TaskManager::GetInstance();

  task_manager.ParallelFor(
      src.size(),
      [src, dst, logn](const TaskInfo& task_info) {
        for (size_t k = task_info.start_idx; k < task_info.end_idx; ++k) {
          const size_t rk = BitReverse(k, logn);
          dst[rk] = src[k];
        }
      },
      src.size(), min_work_chunk);
}

}  // namespace starkware
