#include "starkware/proof_system/proof_system.h"

#include <stdexcept>

#include "gtest/gtest.h"

#include "starkware/error_handling/error_handling.h"

namespace starkware {
namespace {

TEST(FalseOnError, Correctness) {
  EXPECT_TRUE(FalseOnError([]() { ASSERT_RELEASE(true, ""); }));
  EXPECT_FALSE(FalseOnError([]() { ASSERT_RELEASE(false, ""); }));
  EXPECT_THROW(FalseOnError([]() { throw std::invalid_argument(""); }), std::invalid_argument);
}

}  // namespace
}  // namespace starkware
