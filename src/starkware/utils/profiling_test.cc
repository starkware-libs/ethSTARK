#include "starkware/utils/profiling.h"

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::HasSubstr;

TEST(Profiling, DoubleClose) {
  FLAGS_v = 1;
  ProfilingBlock profiling_block("test block");
  EXPECT_NO_THROW(profiling_block.CloseBlock());
  EXPECT_ASSERT(
      profiling_block.CloseBlock(), HasSubstr("ProfilingBlock.CloseBlock() called twice."));
  FLAGS_v = 0;
}

}  // namespace
}  // namespace starkware
