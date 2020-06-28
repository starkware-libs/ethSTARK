#include <sys/resource.h>
#include <cstdio>

#include "benchmark/benchmark.h"
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "starkware/utils/task_manager.h"

int main(int argc, char** argv) {
  // Initializes Google's flags, logging, and benchmark libraries.
  // Use " -- " to separate between glog and gbenchmarks args.
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);  // NOLINT
  ::benchmark::Initialize(&argc, argv);
  LOG(INFO) << "Running Starkware's benchmark with n_threads=" << FLAGS_n_threads << "\n";
  if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return 1;
  }
  ::benchmark::RunSpecifiedBenchmarks();

  return 0;
}
