#include "starkware/algebra/polynomials.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/base_field_element.h"

namespace starkware {
namespace {

TEST(HornerEval, Correctness) {
  Prng prng;
  const auto point = BaseFieldElement::RandomElement(&prng);
  const auto coefs = prng.RandomFieldElementVector<BaseFieldElement>(4);
  EXPECT_EQ(HornerEval(point, std::vector<BaseFieldElement>{}), BaseFieldElement::Zero());
  EXPECT_EQ(HornerEval(point, std::vector<BaseFieldElement>{coefs[0]}), coefs[0]);
  EXPECT_EQ(
      HornerEval(point, coefs),
      coefs[0] + point * coefs[1] + coefs[2] * Pow(point, 2) + coefs[3] * Pow(point, 3));
}

TEST(BatchHornerEval, Correctness) {
  Prng prng;
  const size_t degree = Pow2(prng.UniformInt(0, 4)) - 1;
  auto coefs = prng.RandomFieldElementVector<BaseFieldElement>(degree);
  coefs.push_back(RandomNonZeroElement<BaseFieldElement>(&prng));

  const size_t n_points = prng.UniformInt(0, 10);
  const auto points = prng.RandomFieldElementVector<BaseFieldElement>(n_points);
  std::vector<BaseFieldElement> results(n_points, BaseFieldElement::Zero());
  BatchHornerEval<BaseFieldElement, BaseFieldElement>(points, coefs, results);

  for (size_t i = 0; i < n_points; ++i) {
    ASSERT_EQ(results[i], HornerEval(points[i], coefs));
  }
}

}  // namespace
}  // namespace starkware
