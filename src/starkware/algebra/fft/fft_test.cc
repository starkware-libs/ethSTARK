#include "starkware/algebra/fft/fft.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {
namespace {

/*
  For a given degree_bound, define a random polynomial of degree (degree_bound-1). Compute
  the evaluations of the interpolation polynomial over a random domain and check the correctness of
  the IFFT in both natural and bit-reversed order scenarios.
*/
TEST(IFftTest, Correctness) {
  const size_t log_degree = 4;
  const uint64_t degree = Pow2(log_degree);
  Prng prng;

  const BaseFieldElement offset = BaseFieldElement::RandomElement(&prng);
  const BaseFieldElement gen = GetSubGroupGenerator(degree);
  const auto coefs = prng.RandomFieldElementVector<BaseFieldElement>(degree);
  std::vector<BaseFieldElement> values;
  auto x = offset;
  for (size_t i = 0; i < coefs.size(); ++i) {
    values.push_back(HornerEval(x, coefs));
    x *= gen;
  }

  // Output of the IFFT is multiplied by Pow(2, log_degree), we fix this with the normalizer.
  const BaseFieldElement normalizer = Pow((BaseFieldElement::FromUint(2)).Inverse(), log_degree);
  std::vector<BaseFieldElement> res = BaseFieldElement::UninitializedVector(degree);

  Ifft<BaseFieldElement>(values, res, gen, offset, /*eval_in_natural_order=*/true);
  res = BitReverseVector<BaseFieldElement>(res);
  for (size_t i = 0; i < degree; ++i) {
    EXPECT_EQ(coefs[i], res[i] * normalizer);
  }

  Ifft<BaseFieldElement>(
      BitReverseVector<BaseFieldElement>(values), res, gen, offset,
      /*eval_in_natural_order=*/false);
  for (size_t i = 0; i < degree; ++i) {
    EXPECT_EQ(coefs[i], res[i] * normalizer);
  }
}

/*
  For a given degree_bound, define a random polynomial of degree (degree_bound-1). Compute
  the evaluations of the interpolation polynomial over a random domain and check the correctness of
  the FFT in both natural and bit-reversed order scenarios.
*/
TEST(FftTest, Correctness) {
  const size_t log_degree = 5;
  const uint64_t degree = Pow2(log_degree);
  Prng prng;

  const BaseFieldElement offset = BaseFieldElement::RandomElement(&prng);
  const BaseFieldElement gen = GetSubGroupGenerator(degree);
  const auto coefs = prng.RandomFieldElementVector<BaseFieldElement>(degree);
  std::vector<BaseFieldElement> res = BaseFieldElement::UninitializedVector(degree);

  Fft<BaseFieldElement>(
      BitReverseVector<BaseFieldElement>(coefs), res, gen, offset, /*eval_in_natural_order=*/true);
  ASSERT_EQ(res.size(), coefs.size());
  auto x = offset;
  for (auto y : res) {
    ASSERT_EQ(y, HornerEval(x, coefs));
    x *= gen;
  }

  Fft<BaseFieldElement>(coefs, res, gen, offset, /*eval_in_natural_order=*/false);
  ASSERT_EQ(res.size(), coefs.size());
  x = offset;
  for (auto y : BitReverseVector<BaseFieldElement>(res)) {
    ASSERT_EQ(y, HornerEval(x, coefs));
    x *= gen;
  }
}

TEST(NaturalEvaluationTest, Identity) {
  const size_t log_degree = 4;
  const uint64_t degree = Pow2(log_degree);
  Prng prng;

  const BaseFieldElement gen = GetSubGroupGenerator(degree);
  const BaseFieldElement offset = BaseFieldElement::RandomElement(&prng);
  const auto values = prng.RandomFieldElementVector<BaseFieldElement>(degree);
  std::vector<BaseFieldElement> res = BaseFieldElement::UninitializedVector(degree);
  // Output of the IFFT is multiplied by Pow(2, log_degree), we fix this with the normalizer.
  const BaseFieldElement normalizer = BaseFieldElement::FromUint(degree).Inverse();

  Ifft<BaseFieldElement>(values, res, gen, offset, /*eval_in_natural_order=*/true);
  Fft<BaseFieldElement>(res, res, gen, offset, /*eval_in_natural_order=*/true);
  ASSERT_EQ(res.size(), degree);
  for (size_t i = 0; i < degree; ++i) {
    EXPECT_EQ(res[i] * normalizer, values[i]);
  }
}

TEST(ReverseEvaluationTest, Identity) {
  const size_t log_degree = 4;
  const uint64_t degree = Pow2(log_degree);
  Prng prng;

  const BaseFieldElement gen = GetSubGroupGenerator(degree);
  const BaseFieldElement offset = BaseFieldElement::RandomElement(&prng);
  std::vector<BaseFieldElement> values = prng.RandomFieldElementVector<BaseFieldElement>(degree);
  std::vector<BaseFieldElement> res = BaseFieldElement::UninitializedVector(degree);
  // Output of the IFFT is multiplied by Pow(2, log_degree), we fix this with the normalizer.
  const BaseFieldElement normalizer = BaseFieldElement::FromUint(degree).Inverse();

  Ifft<BaseFieldElement>(
      BitReverseVector<BaseFieldElement>(values), res, gen, offset,
      /*eval_in_natural_order=*/false);
  Fft<BaseFieldElement>(res, res, gen, offset, /*eval_in_natural_order=*/false);
  res = BitReverseVector<BaseFieldElement>(res);
  ASSERT_EQ(res.size(), degree);
  for (size_t i = 0; i < degree; ++i) {
    EXPECT_EQ(res[i] * normalizer, values[i]);
  }
}

}  // namespace
}  // namespace starkware
