#include "starkware/algebra/fields/extension_field_element.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

ExtensionFieldElement ElementFromInts(uint64_t coef0, uint64_t coef1, uint64_t coef2) {
  return ExtensionFieldElement(
      BaseFieldElement::FromUint(coef0), BaseFieldElement::FromUint(coef1),
      BaseFieldElement::FromUint(coef2));
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
  const auto e = BaseFieldElement::RandomElement(&prng);
  const auto f = BaseFieldElement::RandomElement(&prng);
  const auto x = ExtensionFieldElement(a, b, c);
  const auto y = ExtensionFieldElement(d, e, f);
  EXPECT_EQ(x + y, ExtensionFieldElement(a + d, b + e, c + f));
}

TEST(ExtensionFieldElement, Subtraction) {
  Prng prng;
  const auto a = BaseFieldElement::RandomElement(&prng);
  const auto b = BaseFieldElement::RandomElement(&prng);
  const auto c = BaseFieldElement::RandomElement(&prng);
  const auto d = BaseFieldElement::RandomElement(&prng);
  const auto e = BaseFieldElement::RandomElement(&prng);
  const auto f = BaseFieldElement::RandomElement(&prng);
  const auto x = ExtensionFieldElement(a, b, c);
  const auto y = ExtensionFieldElement(d, e, f);
  const auto z = ExtensionFieldElement(a - d, b - e, c - f);
  EXPECT_EQ(x - y, z);
  EXPECT_EQ(y - x, -z);
}

TEST(ExtensionFieldElement, Inverse) {
  Prng prng;
  const auto a = ElementFromInts(467630292337398741, 906471533792748794, 1872395317058213941);
  const auto b = ExtensionFieldElement::RandomElement(&prng);
  const auto a_inv = ElementFromInts(113616337778124578, 2148829266976212440, 823102135911546023);
  EXPECT_EQ(a.Inverse(), a_inv);
  EXPECT_EQ(a, a_inv.Inverse());
  EXPECT_EQ(b, b.Inverse().Inverse());
}

TEST(ExtensionFieldElement, Multiplication) {
  const auto a = ElementFromInts(1389189438653140746, 445910418135829845, 1489643904468381150);
  const auto b = ElementFromInts(878605336020887711, 236494478941238740, 1720938800658033758);
  const auto c = ElementFromInts(691494123816333239, 1595791163912696124, 249432279528055098);
  EXPECT_EQ(a * b, c);
  EXPECT_EQ(a, a * b * b.Inverse());
}

TEST(ExtensionFieldElement, Division) {
  const auto a = ElementFromInts(1094970746620602868, 1431970248202457468, 2275047933470957971);
  const auto b = ElementFromInts(1040062165632977465, 1189102633418973746, 852242137970464510);
  const auto c = ElementFromInts(1593156390489867871, 1962826509931087711, 309378942270673256);
  const auto a_div_b = a / b;
  EXPECT_EQ(a_div_b, c);
  EXPECT_EQ(a, a_div_b * b);
}

TEST(ExtensionFieldElement, Frobenius) {
  Prng prng;

  // Test that Frobenius is the identity on base field elements.
  const auto a = ExtensionFieldElement(BaseFieldElement::RandomElement(&prng));
  EXPECT_EQ(a, a.GetFrobenius());

  // Test that Frobenius order in the Galois group is 3.
  const auto b = ExtensionFieldElement::RandomElement(&prng);
  const auto conj1 = b.GetFrobenius();
  const auto conj2 = conj1.GetFrobenius();
  EXPECT_NE(b, conj1);
  EXPECT_NE(b, conj2);
  EXPECT_EQ(b, conj2.GetFrobenius());

  // Test that symmetric polynomials are invariant to Galois group.
  EXPECT_TRUE((b + conj1 + conj2).IsInBaseField());
  EXPECT_TRUE((b * conj1 + b * conj2 + conj1 * conj2).IsInBaseField());
  EXPECT_TRUE((b * conj1 * conj2).IsInBaseField());

  // Test that Frobenius is a field automorphism.
  const auto c = ExtensionFieldElement::RandomElement(&prng);
  EXPECT_EQ((b + c).GetFrobenius(), b.GetFrobenius() + c.GetFrobenius());
  EXPECT_EQ((b * c).GetFrobenius(), b.GetFrobenius() * c.GetFrobenius());
}

}  // namespace
}  // namespace starkware
