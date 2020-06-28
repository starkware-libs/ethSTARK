#include "starkware/utils/bit_reversal.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

TEST(BitReverse, Integer) {
  EXPECT_EQ(BitReverse(0b1, 4), 0b1000U);
  EXPECT_EQ(BitReverse(0b1101, 4), 0b1011U);
  EXPECT_EQ(BitReverse(0b1101, 6), 0b101100U);
  EXPECT_EQ(BitReverse(0xffffffffefecc8e7L, 64), 0xe71337f7ffffffffL);
}

TEST(BitReverse, BadInput) {
  EXPECT_ASSERT(
      BitReverse(0xffffffffefecc8e7, 62),
      testing::HasSubstr("n must be smaller than 2^number_of_bits."));
}

TEST(BitReverseVector, BadInput) {
  const size_t n = 3;
  auto src = BaseFieldElement::UninitializedVector(n);
  auto dst = BaseFieldElement::UninitializedVector(n);
  EXPECT_ASSERT(
      BitReverseVector<BaseFieldElement>(src, dst), testing::HasSubstr("n must be a power of 2."));

  dst = BaseFieldElement::UninitializedVector(n + 1);
  EXPECT_ASSERT(
      BitReverseVector<BaseFieldElement>(src, dst),
      testing::HasSubstr("Span sizes of src and dst must be similar."));
}

TEST(BitReverse, RandomInteger) {
  Prng prng;
  auto logn = prng.template UniformInt<size_t>(1, 63);
  auto val = prng.template UniformInt<uint64_t>(0, Pow2(logn) - 1);
  EXPECT_EQ(val, BitReverse(BitReverse(val, logn), logn));
}

TEST(BitReverseVector, Correctness) {
  const size_t log_n = 3;
  const uint64_t n = Pow2(log_n);
  auto src = BaseFieldElement::UninitializedVector(n);
  auto dst = BaseFieldElement::UninitializedVector(n);

  for (size_t i = 0; i < n; ++i) {
    src[i] = BaseFieldElement::FromUint(i);
  }
  BitReverseVector<BaseFieldElement>(src, dst);
  for (size_t i = 0; i < n; ++i) {
    EXPECT_EQ(BaseFieldElement(src[i]), dst[BitReverse(i, log_n)]);
  }

  dst = BitReverseVector<BaseFieldElement>(src);
  for (size_t i = 0; i < n; ++i) {
    EXPECT_EQ(BaseFieldElement(src[i]), dst[BitReverse(i, log_n)]);
  }
}

}  // namespace
}  // namespace starkware
