#include "starkware/algebra/fields/extension_field_element.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

ExtensionFieldElement ElementFromInts(uint64_t coef0, uint64_t coef1) {
  return ExtensionFieldElement(
      BaseFieldElement::FromUint(coef0), BaseFieldElement::FromUint(coef1));
}

TEST(ExtensionFieldElement, Equality) {
  Prng prng;
  const auto a = ExtensionFieldElement::RandomElement(&prng);
  const auto b = ExtensionFieldElement::RandomElement(&prng);
  EXPECT_TRUE(a == a);
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(b == b);
  EXPECT_TRUE(b != a);
}

TEST(ExtensionFieldElement, Addition) {
  Prng prng;
  const auto a = BaseFieldElement::RandomElement(&prng);
  const auto b = BaseFieldElement::RandomElement(&prng);
  const auto c = BaseFieldElement::RandomElement(&prng);
  const auto d = BaseFieldElement::RandomElement(&prng);
  const auto x = ExtensionFieldElement(a, b);
  const auto y = ExtensionFieldElement(c, d);
  EXPECT_EQ(x + y, ExtensionFieldElement(a + c, b + d));
}

TEST(ExtensionFieldElement, Subtraction) {
  Prng prng;
  const auto a = BaseFieldElement::RandomElement(&prng);
  const auto b = BaseFieldElement::RandomElement(&prng);
  const auto c = BaseFieldElement::RandomElement(&prng);
  const auto d = BaseFieldElement::RandomElement(&prng);
  const auto x = ExtensionFieldElement(a, b);
  const auto y = ExtensionFieldElement(c, d);
  const auto z = ExtensionFieldElement(a - c, b - d);
  EXPECT_EQ(x - y, z);
  EXPECT_EQ(y - x, -z);
}

TEST(ExtensionFieldElement, Inverse) {
  Prng prng;
  const auto a = ElementFromInts(1512385002782, 1518554002782);
  const auto b = ExtensionFieldElement::RandomElement(&prng);
  const auto a_inv = ElementFromInts(627298716069510131, 452096764909914143);
  EXPECT_EQ(a.Inverse(), a_inv);
  EXPECT_EQ(a, a_inv.Inverse());
  EXPECT_EQ(b, b.Inverse().Inverse());
}

TEST(ExtensionFieldElement, Multiplication) {
  Prng prng;
  const auto a = ElementFromInts(15185002782, 151825002782);
  const auto b = ElementFromInts(1523185002782, 3415185002782);
  const auto c = ElementFromInts(2109697304335391094, 1281585540691370122);
  EXPECT_EQ(a * b, c);
  EXPECT_EQ(a, a * b * b.Inverse());
}

TEST(ExtensionFieldElement, MultiplicationZeroCoefs) {
  Prng prng;
  const auto a = ElementFromInts(234234515185002782, 0);
  const auto b = ElementFromInts(124545345232342343, 731157439330712351);
  const auto c = ElementFromInts(945362483797116086, 1219632311329118709);
  const auto d = ElementFromInts(2277073594214247884, 0);
  EXPECT_EQ(a * b, c);
  EXPECT_EQ(b * a, c);
  EXPECT_EQ(a * a, d);
  EXPECT_EQ(a, a * b * b.Inverse());
  EXPECT_EQ(b, b * a * a.Inverse());
}

TEST(ExtensionFieldElement, Division) {
  Prng prng;
  const auto a = ElementFromInts(151823002782, 151850027824519);
  const auto b = ElementFromInts(15185002782078, 18151850027827);
  const auto c = ElementFromInts(381106914516768379, 979171161076691485);
  const auto a_div_b = a / b;
  EXPECT_EQ(a_div_b, c);
  EXPECT_EQ(a, a_div_b * b);
}

}  // namespace
}  // namespace starkware
