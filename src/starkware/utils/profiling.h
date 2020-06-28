#ifndef STARKWARE_UTILS_PROFILING_H_
#define STARKWARE_UTILS_PROFILING_H_

#include <chrono>
#include <string>

namespace starkware {

/*
  This class is used to annotate different stages in the prover.
  The class enables logging the prover's current stage.

  In order to see the logs, pass the cmd line args: -v=1 --logtostderr.

  The class can be used in scoped RAII-style:
  int res;
  {
    ProfilingBlock profiling_block("compute res");
    res = compute_res()
  }

  Or with manual block closing when scoped RAII-style is inconvenient:
  ProfilingBlock profiling_block("compute res");
  auto res = compute_res();
  profiling_block.CloseBlock();
*/
class ProfilingBlock {
 public:
  explicit ProfilingBlock(std::string description);

  ~ProfilingBlock();

  /*
    Close the ProfilingBlock block early, in case  RAII is inconvenient.
  */
  void CloseBlock();

  ProfilingBlock(const ProfilingBlock&) = delete;
  ProfilingBlock& operator=(const ProfilingBlock&) = delete;
  ProfilingBlock(ProfilingBlock&& other) = delete;
  ProfilingBlock& operator=(ProfilingBlock&& other) = delete;

 private:
  // The start time of the stage.
  std::chrono::time_point<std::chrono::system_clock> start_time_;
  // A short description of the stage.
  const std::string description_;
  // Whether the profiling block is opened or closed.
  bool closed_ = false;
};

}  // namespace starkware

#endif  // STARKWARE_UTILS_PROFILING_H_
