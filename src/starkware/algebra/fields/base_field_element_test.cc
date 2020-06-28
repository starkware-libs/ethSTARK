#include "starkware/algebra/fields/base_field_element.h"

#include "gtest/gtest.h"

namespace starkware {
namespace {

// These are tests of functions that are unique to BaseFieldElement. Note there are many other tests
// in field_operations_test.cc .

TEST(BaseFieldElement, ToStandardForm) {
  Prng prng;
  ASSERT_EQ(BaseFieldElement::FromUint(0).ToStandardForm(), 0);
  ASSERT_EQ(BaseFieldElement::FromUint(1).ToStandardForm(), 1);

  uint64_t random_int = prng.UniformInt<uint64_t>(0, 1000000);
  ASSERT_EQ(BaseFieldElement::FromUint(random_int).ToStandardForm(), random_int);
}

TEST(BaseFieldElement, Constexpr) {
  constexpr BaseFieldElement kV = BaseFieldElement::FromUint(15);
  constexpr BaseFieldElement kVInv = kV.Inverse();
  EXPECT_EQ(kV * kVInv, BaseFieldElement::One());
}

TEST(BaseFieldElement, kModulusBits) {
  constexpr uint64_t kMsb = Pow2(BaseFieldElement::kModulusBits);
  constexpr uint64_t kUnusedMask = ~((kMsb << 1) - 1);

  static_assert((BaseFieldElement::kModulus & kMsb) == kMsb, "MSB should be set.");
  static_assert((BaseFieldElement::kModulus & kUnusedMask) == 0, "Unused bits should be cleared.");
}

}  // namespace
}  // namespace starkware
