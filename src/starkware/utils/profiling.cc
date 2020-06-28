#include "starkware/utils/profiling.h"

#include <utility>

#include "glog/logging.h"

#include "starkware/error_handling/error_handling.h"

namespace starkware {

namespace {

// Commnand line argument -v should be at least 1 to enable profiling.
constexpr int kVlog = 1;

// Start time of the prover.
auto program_start = std::chrono::system_clock::now();

/*
  Appends the duration of the phase to a given output stream.
*/
template <typename Duration>
void PrintDuration(std::ostream* os, Duration d) {
  std::chrono::duration<double> sec = d;

  *os << sec.count() << " sec";
}

}  // namespace

ProfilingBlock::ProfilingBlock(std::string description)
    : start_time_(std::chrono::system_clock::now()), description_(std::move(description)) {
  if (FLAGS_v < kVlog) {
    return;
  }
  std::stringstream os;

  // Print the duration since the start time of the prover (only if not prepending the log prefix to
  // the start of each log line).
  if (!FLAGS_log_prefix) {
    auto now = std::chrono::system_clock::now();
    PrintDuration(&os, now - program_start);
    os << ": ";
  }

  os << description_ << " started";
  VLOG(kVlog) << os.str();
}

ProfilingBlock::~ProfilingBlock() {
  if (!closed_) {
    CloseBlock();
  }
}

void ProfilingBlock::CloseBlock() {
  if (FLAGS_v < kVlog) {
    return;
  }
  ASSERT_RELEASE(!closed_, "ProfilingBlock.CloseBlock() called twice.");

  std::stringstream os;
  auto now = std::chrono::system_clock::now();

  // Print the duration since the start time of the prover (only if not prepending the log prefix to
  // the start of each log line).
  if (!FLAGS_log_prefix) {
    PrintDuration(&os, now - program_start);
    os << ": ";
  }

  os << description_ << " finished in ";
  PrintDuration(&os, now - start_time_);
  VLOG(kVlog) << os.str();
  closed_ = true;
}

}  // namespace starkware
