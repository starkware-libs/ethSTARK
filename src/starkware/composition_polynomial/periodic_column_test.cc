#include "starkware/composition_polynomial/periodic_column.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fft/fft.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {
namespace {

using testing::HasSubstr;

/*
  Creates a PeriodicColumn with the given parameters, and performs the following
  tests:
    1. EvalAtPoint() and the iterator returned by GetCoset(), on the original coset, return the
       original values.
    2. EvalAtPoint() and the iterator returned by GetCoset(), on a random coset, are consistent with
       the interpolation polynomial computed using an IFFT.
*/
void TestPeriodicColumn(const size_t n_values, const size_t coset_size, Prng* prng) {
  const BaseFieldElement group_generator = GetSubGroupGenerator(coset_size);
  const BaseFieldElement offset = BaseFieldElement::One();
  const std::vector<BaseFieldElement> values =
      prng->RandomFieldElementVector<BaseFieldElement>(n_values);
  PeriodicColumn column(values, coset_size);

  BaseFieldElement point = offset;
  std::vector<BaseFieldElement> values_ext;  // Contains coset_size/n_values repetitions of values.
  auto periodic_column_coset = column.GetCoset(offset, coset_size);
  auto it = periodic_column_coset.begin();
  for (size_t i = 0; i < coset_size; ++i) {
    const BaseFieldElement iterator_value = *it;

    // Check that the iterator agrees with EvalAtPoint.
    const BaseFieldElement expected_val = values[i % values.size()];
    ASSERT_EQ(expected_val, iterator_value);
    ASSERT_EQ(iterator_value, column.EvalAtPoint(point));

    values_ext.push_back(iterator_value);
    point *= group_generator;
    ++it;
  }

  // Check that this is indeed an extrapolation of a low-degree polynomial, by computing the
  // expected polynomial and substituting a random point.
  auto interpolant = BaseFieldElement::UninitializedVector(values_ext.size());
  Ifft<BaseFieldElement>(
      values_ext, interpolant, group_generator, offset, /*eval_in_natural_order=*/true);
  // Normalize the output of the IFFT.
  const auto lde_size_inverse = BaseFieldElement::FromUint(interpolant.size()).Inverse();
  for (auto& elem : interpolant) {
    elem *= lde_size_inverse;
  }
  interpolant = BitReverseVector<BaseFieldElement>(interpolant);
  const BaseFieldElement random_point = BaseFieldElement::RandomElement(prng);
  EXPECT_EQ(HornerEval(random_point, interpolant), column.EvalAtPoint(random_point));

  // Check the iterator on a random coset.
  point = random_point;
  auto periodic_column_coset2 = column.GetCoset(random_point, coset_size);
  auto it2 = periodic_column_coset2.begin();
  for (size_t i = 0; i < coset_size; ++i) {
    ASSERT_EQ(HornerEval(point, interpolant), *it2);
    point *= group_generator;
    ++it2;
  }
}

TEST(PeriodicColumn, Correctness) {
  Prng prng;
  TestPeriodicColumn(8, 32, &prng);
  TestPeriodicColumn(8, 8, &prng);
  TestPeriodicColumn(1, 8, &prng);
  TestPeriodicColumn(1, 1, &prng);

  // Random sizes.
  const size_t log_coset_size = prng.UniformInt(0, 5);
  const auto log_n_values = prng.template UniformInt<size_t>(0, log_coset_size);
  TestPeriodicColumn(Pow2(log_n_values), Pow2(log_coset_size), &prng);
}

TEST(PeriodicColumn, NumberOfValuesMustDivideCosetSize) {
  Prng prng;
  EXPECT_ASSERT(TestPeriodicColumn(3, 32, &prng), HasSubstr("with a remainder"));
}

TEST(PeriodicColumn, InvalidCosetSize) {
  Prng prng;
  EXPECT_ASSERT(TestPeriodicColumn(13, 26, &prng), HasSubstr("must be a power of 2"));
}

}  // namespace
}  // namespace starkware
