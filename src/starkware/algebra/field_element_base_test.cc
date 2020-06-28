#include "starkware/algebra/field_element_base.h"

#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

template <typename T>
class FieldElementBaseTest : public ::testing::Test {};

using TestedFieldTypes = ::testing::Types<BaseFieldElement, ExtensionFieldElement>;
TYPED_TEST_CASE(FieldElementBaseTest, TestedFieldTypes);

TYPED_TEST(FieldElementBaseTest, ToString) {
  using FieldElementT = TypeParam;
  Prng prng;
  auto x = FieldElementT::RandomElement(&prng);
  std::stringstream s;
  s << x;
  EXPECT_EQ(x.ToString(), s.str());
}

TYPED_TEST(FieldElementBaseTest, AdditionAssignmentOperator) {
  using FieldElementT = TypeParam;
  Prng prng;
  FieldElementT x = FieldElementT::RandomElement(&prng);
  FieldElementT y = FieldElementT::RandomElement(&prng);

  FieldElementT tmp = x;
  tmp += y;
  EXPECT_EQ(x + y, tmp);
}

TYPED_TEST(FieldElementBaseTest, MultiplicationAssignmentOperator) {
  using FieldElementT = TypeParam;
  Prng prng;
  FieldElementT x = FieldElementT::RandomElement(&prng);
  FieldElementT y = FieldElementT::RandomElement(&prng);

  FieldElementT tmp = x;
  tmp *= y;
  EXPECT_EQ(x * y, tmp);
}

TYPED_TEST(FieldElementBaseTest, Correctness) {
  using FieldElementT = TypeParam;
  EXPECT_TRUE(kIsFieldElement<FieldElementT>);
}

TEST(IsFieldElement, NegativeTest) { EXPECT_FALSE(kIsFieldElement<int>); }

}  // namespace
}  // namespace starkware
