#include "starkware/algebra/domains/coset.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::HasSubstr;

TEST(CosetTests, SingleElement) {
  const Coset coset(1U, BaseFieldElement::One());
  EXPECT_EQ(coset.Size(), 1U);
  EXPECT_EQ(coset.Generator(), BaseFieldElement::One());
}

TEST(CosetTests, TwoElements) {
  const Coset coset(2U, BaseFieldElement::One());
  EXPECT_EQ(coset.Size(), 2U);
  EXPECT_EQ(coset.Generator(), -BaseFieldElement::One());
}

TEST(CosetTests, RandomOffsetSize1024) {
  Prng prng;
  const size_t log_coset_size = 10;
  const size_t coset_size = Pow2(log_coset_size);
  const auto source_eval_offset = BaseFieldElement::RandomElement(&prng);
  const Coset coset(coset_size, source_eval_offset);
  EXPECT_EQ(coset.Size(), coset_size);
  BaseFieldElement curr_coset_element = coset.Offset() * coset.Generator();
  for (size_t i = 1; i < coset_size; ++i) {
    ASSERT_NE(curr_coset_element, BaseFieldElement::One());
    curr_coset_element = curr_coset_element * coset.Generator();
  }
  EXPECT_EQ(curr_coset_element, source_eval_offset);
}

TEST(CosetTests, Size13) {
  EXPECT_ASSERT(Coset(13, BaseFieldElement::One()), HasSubstr("must be a power of 2"));
}

}  // namespace
}  // namespace starkware
