#include "starkware/randomness/prng.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include "glog/logging.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/hash_chain.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/to_from_string.h"

namespace starkware {
namespace {

template <typename T>
class PrngTest : public ::testing::Test {};

using IntTypes = ::testing::Types<uint16_t, uint32_t, uint64_t>;
TYPED_TEST_CASE(PrngTest, IntTypes);

template <typename T>
class PrngFieldTest : public ::testing::Test {};
using FieldTypes = ::testing::Types<BaseFieldElement>;
TYPED_TEST_CASE(PrngFieldTest, FieldTypes);

TYPED_TEST(PrngTest, TwoInvocationsAreNotIdentical) {
  Prng prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  auto a = prng.UniformInt<TypeParam>(0, std::numeric_limits<TypeParam>::max());
  auto b = prng.UniformInt<TypeParam>(0, std::numeric_limits<TypeParam>::max());
  EXPECT_NE(a, b);
}

TYPED_TEST(PrngTest, VectorInvocation) {
  Prng prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  auto v = prng.UniformIntVector<TypeParam>(0, std::numeric_limits<TypeParam>::max(), 10);
  auto w = prng.UniformIntVector<TypeParam>(0, std::numeric_limits<TypeParam>::max(), 10);

  ASSERT_EQ(v.size(), 10U);
  ASSERT_EQ(w.size(), 10U);
  for (size_t i = 0; i < 10; ++i) {
    EXPECT_NE(v.at(i), w.at(i));
  }

  auto z = prng.UniformIntVector<TypeParam>(0, std::numeric_limits<TypeParam>::max(), 10000);
  TypeParam max = 0;
  for (TypeParam num : z) {
    max = std::max(max, num);
  }
  EXPECT_NEAR(
      std::numeric_limits<TypeParam>::max(), max,
      std::numeric_limits<TypeParam>::max() * 10000.0 / (10000 + 1));
}

TEST(PrngTest, BadInput) {
  Prng prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  EXPECT_ASSERT(
      prng.UniformIntVector<uint8_t>(std::numeric_limits<uint8_t>::max(), 0, 10),
      testing::HasSubstr("Invalid interval."));
  EXPECT_ASSERT(
      prng.UniformInt<uint8_t>(std::numeric_limits<uint8_t>::max(), 0),
      testing::HasSubstr("Invalid interval."));
}

/*
  1. Initializes Prng with seed.
  2. Generates a list of numbers.
  3. Reseeds with the same seed and checks that a second sequence of numbers is identical to the
     first one.
*/
TYPED_TEST(PrngTest, ReseedingWithSameSeedYieldsSameRandomness) {
  unsigned size = 100;
  std::array<std::byte, 5> seed = MakeByteArray<1, 2, 3, 4, 5>();
  std::vector<TypeParam> vals(size);
  Prng prng(seed);
  std::transform(vals.begin(), vals.end(), vals.begin(), [&prng](const TypeParam& /* unused */) {
    return prng.UniformInt<TypeParam>(0, std::numeric_limits<TypeParam>::max());
  });
  Prng new_prng(seed);
  for (auto& val : vals) {
    auto new_val = new_prng.UniformInt<TypeParam>(0, std::numeric_limits<TypeParam>::max());
    ASSERT_EQ(val, new_val);
  }
}

TEST(PrngTest, Clone) {
  const unsigned size = 100;
  Prng prng1;
  Prng prng2 = prng1.Clone();
  const std::vector<std::byte> vec1 = prng1.RandomByteVector(size);
  const std::vector<std::byte> vec2 = prng2.RandomByteVector(size);
  EXPECT_EQ(vec1, vec2);

  Prng prng3 = prng1.Clone();
  const std::vector<std::byte> vec3 = prng3.RandomByteVector(size);
  const std::vector<std::byte> vec4 = prng1.RandomByteVector(size);
  EXPECT_EQ(vec3, vec4);
}

TEST(PrngTest, CloneExplicitSeed) {
  const unsigned size = 100;
  Prng prng1(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  Prng prng2 = prng1.Clone();
  const std::vector<std::byte> vec1 = prng1.RandomByteVector(size);
  const std::vector<std::byte> vec2 = prng2.RandomByteVector(size);
  EXPECT_EQ(vec1, vec2);

  Prng prng3 = prng1.Clone();
  const std::vector<std::byte> vec3 = prng3.RandomByteVector(size);
  const std::vector<std::byte> vec4 = prng1.RandomByteVector(size);
  EXPECT_EQ(vec3, vec4);
}

TEST(PrngTest, MoveConstructor) {
  const unsigned size = 100;
  Prng prng1(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  Prng prng2(Prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>()));
  const std::vector<std::byte> vec1 = prng1.RandomByteVector(size);
  const std::vector<std::byte> vec2 = prng2.RandomByteVector(size);
  EXPECT_EQ(vec1, vec2);

  Prng prng3(std::move(prng1));
  const std::vector<std::byte> vec3 = prng3.RandomByteVector(size);
  const std::vector<std::byte> vec4 = prng2.RandomByteVector(size);
  EXPECT_EQ(vec3, vec4);
}

TEST(PrngTest, MoveAssignment) {
  const unsigned size = 100;
  Prng prng1(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  Prng prng2;
  prng2 = Prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  const std::vector<std::byte> vec1 = prng1.RandomByteVector(size);
  const std::vector<std::byte> vec2 = prng2.RandomByteVector(size);
  EXPECT_EQ(vec1, vec2);

  Prng prng3;
  prng3 = std::move(prng1);
  const std::vector<std::byte> vec3 = prng3.RandomByteVector(size);
  const std::vector<std::byte> vec4 = prng2.RandomByteVector(size);
  EXPECT_EQ(vec3, vec4);

  Prng prng5;
  const std::vector<std::byte> vec_different = prng5.RandomByteVector(size);
  prng5 = std::move(prng3);
  const std::vector<std::byte> vec5 = prng5.RandomByteVector(size);
  const std::vector<std::byte> vec6 = prng2.RandomByteVector(size);
  EXPECT_EQ(vec5, vec6);

  EXPECT_NE(vec1, vec_different);
  EXPECT_NE(vec3, vec_different);
  EXPECT_NE(vec5, vec_different);
}

TYPED_TEST(PrngFieldTest, FieldElementVectorInvocation) {
  Prng prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());

  auto v = prng.RandomFieldElementVector<TypeParam>(10);
  auto w = prng.RandomFieldElementVector<TypeParam>(7);
  ASSERT_EQ(v.size(), 10U);
  ASSERT_EQ(w.size(), 7U);
  for (size_t i = 0; i < 7; ++i) {
    EXPECT_NE(v.at(i), w.at(i));
  }
}

TYPED_TEST(PrngFieldTest, VectorSingletonOrEmpty) {
  Prng prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());

  ASSERT_EQ(0U, prng.RandomFieldElementVector<TypeParam>(0).size());
  ASSERT_EQ(1U, prng.RandomFieldElementVector<TypeParam>(1).size());
}

}  // namespace
}  // namespace starkware
