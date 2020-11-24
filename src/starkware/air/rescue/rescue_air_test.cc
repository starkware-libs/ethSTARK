#include "starkware/air/rescue/rescue_air.h"

#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/air_test_utils.h"
#include "starkware/air/rescue/rescue_constants.h"
#include "starkware/composition_polynomial/composition_polynomial.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::HasSubstr;

TEST(RescueAir, PublicInputFromPrivateInput) {
  RescueAir::WitnessT witness;
  witness.reserve(4);
  for (size_t i = 0; i < 16; i += 4) {
    witness.push_back({BaseFieldElement::FromUint(i), BaseFieldElement::FromUint(i + 1),
                       BaseFieldElement::FromUint(i + 2), BaseFieldElement::FromUint(i + 3)});
  }

  // These values were computed using the public code for the STARK-friendly hash challenge at
  // https://starkware.co/hash-challenge-implementation-reference-code/#marvellous .
  // They were validated against the implementation at src/starkware/air/rescue/rescue_hash.py.
  EXPECT_EQ(
      RescueAir::PublicInputFromPrivateInput(witness),
      RescueAir::WordT({
          BaseFieldElement::FromUint(0x87e4b0692b6591b),
          BaseFieldElement::FromUint(0x902c454f0beb94e),
          BaseFieldElement::FromUint(0x82fb00534fdf5d2),
          BaseFieldElement::FromUint(0xf85e4e03a0b7f87),
      }));
}

/*
  This test builds a trace for the Rescue AIR with a hash chain of length 45. If everything is fine,
  the expected output will be contained in row number (32 * 45 / 3 - 1) and there will not be an
  assertion. Then, it computes its low degree extension to an evaluation domain of size 4096,
  applies the Rescue constraint, and finally checks that the result (the evaluations of the
  composition polynomial at the evaluation domain) is of degree (32 * 16 * 4 - 1).
*/
TEST(RescueAir, Correctness) {
  Prng prng;
  const size_t chain_length = 45;

  // Pick a random witness.
  RescueAir::WitnessT witness;
  witness.reserve(chain_length + 1);
  for (size_t i = 0; i < chain_length + 1; ++i) {
    witness.push_back(
        {BaseFieldElement::RandomElement(&prng), BaseFieldElement::RandomElement(&prng),
         BaseFieldElement::RandomElement(&prng), BaseFieldElement::RandomElement(&prng)});
  }

  // Compute the Rescue hash chain.
  const RescueAir::WordT output = RescueAir::PublicInputFromPrivateInput(witness);

  RescueAir rescue_air(
      output, chain_length, /*is_zero_knowledge=*/prng.UniformInt(0, 1) == 1,
      /*n_queries=*/100);

  Trace trace = rescue_air.GetTrace(witness, &prng);
  const uint64_t chain_rows =
      SafeDiv(chain_length, RescueAir::kHashesPerBatch) * RescueAir::kBatchHeight;
  const size_t output_row = (chain_rows - 1) * SafeDiv(trace.Length(), Pow2(Log2Ceil(chain_rows)));
  EXPECT_EQ(
      output, RescueAir::WordT({
                  trace.GetColumn(0)[output_row],
                  trace.GetColumn(1)[output_row],
                  trace.GetColumn(2)[output_row],
                  trace.GetColumn(3)[output_row],
              }));

  {
    RescueAir::WitnessT wrong_witness = witness;
    wrong_witness.push_back(
        {BaseFieldElement::RandomElement(&prng), BaseFieldElement::RandomElement(&prng),
         BaseFieldElement::RandomElement(&prng), BaseFieldElement::RandomElement(&prng)});
    EXPECT_ASSERT(
        rescue_air.GetTrace(wrong_witness, &prng),
        HasSubstr(
            "Witness size is " + std::to_string(chain_length + 2) + ", should be " +
            std::to_string(chain_length + 1)));
  }

  {
    RescueAir::WitnessT wrong_witness = witness;
    auto random_row_index = prng.template UniformInt<size_t>(0, wrong_witness.size() - 1);
    auto random_element_index = prng.template UniformInt<size_t>(0, RescueAir::kWordSize - 1);
    wrong_witness[random_row_index][random_element_index] += BaseFieldElement::One();
    EXPECT_ASSERT(
        rescue_air.GetTrace(wrong_witness, &prng),
        HasSubstr("Given witness is not a correct preimage"));
  }

  const std::vector<ExtensionFieldElement> random_coefficients =
      prng.RandomFieldElementVector<ExtensionFieldElement>(rescue_air.NumRandomCoefficients());

  const uint64_t expected_degree = rescue_air.GetCompositionPolynomialDegreeBound() - 1;
  const uint64_t actual_degree = ComputeCompositionDegree(rescue_air, trace, random_coefficients);
  EXPECT_EQ(actual_degree, expected_degree);
}

TEST(RescueAir, ConstraintsEval) {
  Prng prng;

  const size_t chain_length = 45;

  // Pick a random witness.
  RescueAir::WitnessT witness;
  witness.reserve(chain_length + 1);
  for (size_t i = 0; i < chain_length + 1; ++i) {
    witness.push_back(
        {BaseFieldElement::RandomElement(&prng), BaseFieldElement::RandomElement(&prng),
         BaseFieldElement::RandomElement(&prng), BaseFieldElement::RandomElement(&prng)});
  }

  // Compute the Rescue hash chain.
  const RescueAir::WordT output = RescueAir::PublicInputFromPrivateInput(witness);
  RescueAir rescue_air(
      output, chain_length, /*is_zero_knowledge=*/prng.UniformInt(0, 1) == 1,
      /*n_queries=*/10);

  auto neighbors = prng.RandomFieldElementVector<BaseFieldElement>(2 * RescueAir::kStateSize);
  auto periodic_columns =
      prng.RandomFieldElementVector<BaseFieldElement>(RescueAir::kNumPeriodicColumns);
  auto random_coefficients =
      prng.RandomFieldElementVector<ExtensionFieldElement>(rescue_air.NumRandomCoefficients());
  auto point_powers = prng.RandomFieldElementVector<BaseFieldElement>(10);
  auto shifts = prng.RandomFieldElementVector<BaseFieldElement>(6);

  ExtensionFieldElement eval_as_base = rescue_air.ConstraintsEval<BaseFieldElement>(
      neighbors, {}, periodic_columns, random_coefficients, point_powers, shifts);
  ExtensionFieldElement eval_as_extension = rescue_air.ConstraintsEval<ExtensionFieldElement>(
      std::vector<ExtensionFieldElement>{neighbors.begin(), neighbors.end()}, {},
      std::vector<ExtensionFieldElement>{periodic_columns.begin(), periodic_columns.end()},
      random_coefficients,
      std::vector<ExtensionFieldElement>{point_powers.begin(), point_powers.end()}, shifts);
  EXPECT_EQ(eval_as_base, eval_as_extension);
}

}  // namespace
}  // namespace starkware
