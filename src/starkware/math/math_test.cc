#include "starkware/math/math.h"

#include "gtest/gtest.h"

#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::HasSubstr;

TEST(Math, Pow2) {
  EXPECT_EQ(32U, Pow2(5));
  EXPECT_EQ(1U, Pow2(0));
  EXPECT_EQ(UINT64_C(0x8000000000000000), Pow2(63));
  EXPECT_ASSERT(Pow2(64), HasSubstr("n must be smaller than 64."));
}

TEST(Math, IsPowerOf2Two) {
  EXPECT_TRUE(IsPowerOfTwo(32));
  EXPECT_FALSE(IsPowerOfTwo(31));
  EXPECT_FALSE(IsPowerOfTwo(33));
  EXPECT_FALSE(IsPowerOfTwo(0));
  EXPECT_TRUE(IsPowerOfTwo(1));
}

TEST(Math, Log2Floor) {
  EXPECT_ASSERT(Log2Floor(0), HasSubstr("log2 of 0 is undefined."));
  EXPECT_EQ(Log2Floor(1), 0U);
  EXPECT_EQ(Log2Floor(31), 4U);
  EXPECT_EQ(Log2Floor(32), 5U);
  EXPECT_EQ(Log2Floor(33), 5U);
  EXPECT_EQ(Log2Floor(UINT64_C(0xffffffffffffffff)), 63U);
}

TEST(Math, SafeLog2) {
  EXPECT_ASSERT(SafeLog2(0), HasSubstr("must be a power of 2."));
  EXPECT_EQ(SafeLog2(1), 0U);
  EXPECT_ASSERT(SafeLog2(31), HasSubstr("must be a power of 2."));
  EXPECT_EQ(SafeLog2(32), 5U);
  EXPECT_ASSERT(SafeLog2(33), HasSubstr("must be a power of 2."));
}

TEST(Math, DivCeil) {
  EXPECT_EQ(DivCeil(7U, 3U), 3U);
  EXPECT_EQ(DivCeil(16U, 4U), 4U);
  EXPECT_EQ(DivCeil(17U, 4U), 5U);
  EXPECT_ASSERT(DivCeil(17U, 0U), HasSubstr("The denominator cannot be zero."));
  EXPECT_ASSERT(DivCeil(0U, 0U), HasSubstr("The denominator cannot be zero."));
}

TEST(Math, SafeDiv) {
  EXPECT_EQ(SafeDiv(8U, 4U), 2U);
  EXPECT_ASSERT(SafeDiv(0U, 0U), HasSubstr("The denominator cannot be zero."));
  EXPECT_ASSERT(SafeDiv(8U, 0U), HasSubstr("The denominator cannot be zero."));
  EXPECT_ASSERT(
      SafeDiv(17U, 7U), HasSubstr("The denominator 7 divides the numerator 17 with a remainder."));
  EXPECT_ASSERT(
      SafeDiv(4U, 8U), HasSubstr("The denominator 8 divides the numerator 4 with a remainder."));
  static_assert(
      SafeDiv(8U, 4U) == 2U,
      "The SafeDiv function doesn't perform as expected in compilation time.");
}

TEST(Math, Log2Ceil) {
  EXPECT_EQ(Log2Ceil(1), 0U);
  EXPECT_EQ(Log2Ceil(31), 5U);
  EXPECT_EQ(Log2Ceil(32), 5U);
  EXPECT_EQ(Log2Ceil(33), 6U);
  EXPECT_EQ(Log2Ceil(UINT64_C(0xffffffffffffffff)), 64U);
  EXPECT_ASSERT(Log2Ceil(0), HasSubstr("log2 of 0 is undefined."));
}

}  // namespace
}  // namespace starkware
