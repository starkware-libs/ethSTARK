#include "starkware/air/test_air/test_air.h"

#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/air_test_utils.h"
#include "starkware/air/trace.h"
#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

TEST(TestAirTest, CompositionEndToEnd) {
  Prng prng;
  const uint64_t trace_length = 512;
  const uint64_t res_claim_index = 500;
  const BaseFieldElement private_input(BaseFieldElement::RandomElement(&prng));
  const BaseFieldElement claimed_res(
      TestAir::PublicInputFromPrivateInput(private_input, res_claim_index));
  const TestAir air(
      trace_length, res_claim_index, claimed_res, /*is_zero_knowledge=*/prng.UniformInt(0, 1) == 1,
      /*n_queries=*/10);

  const std::vector<ExtensionFieldElement> random_coefficients =
      prng.RandomFieldElementVector<ExtensionFieldElement>(air.NumRandomCoefficients());

  // Construct trace.
  Trace trace = air.GetTrace(private_input, &prng);

  // Verify composition degree.
  EXPECT_EQ(
      ComputeCompositionDegree(air, trace, random_coefficients),
      air.GetCompositionPolynomialDegreeBound() - 1);

  // Negative case test.
  ASSERT_GT(trace.Width(), 0U);
  size_t slackness_factor = SafeDiv(trace.Length(), trace_length);
  // Randomly change one of the trace's elements and check that the composition degree is indeed
  // greater than its expected bound.
  trace.SetTraceElementForTesting(
      prng.UniformInt<size_t>(0, trace.Width() - 1),
      slackness_factor * prng.UniformInt<size_t>(0, trace_length - 1),
      BaseFieldElement::RandomElement(&prng));

  // Verify composition degree.
  EXPECT_GT(
      ComputeCompositionDegree(air, trace, random_coefficients),
      air.GetCompositionPolynomialDegreeBound() - 1);
}

}  // namespace
}  // namespace starkware
