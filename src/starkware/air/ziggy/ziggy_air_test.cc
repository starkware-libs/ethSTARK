#include "starkware/air/ziggy/ziggy_air.h"

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

TEST(ZiggyAir, PublicInputFromPrivateInput) {
  ZiggyAir::SecretPreimageT secret_preimage{
      BaseFieldElement::FromUint(1), BaseFieldElement::FromUint(2), BaseFieldElement::FromUint(3),
      BaseFieldElement::FromUint(4), BaseFieldElement::FromUint(5), BaseFieldElement::FromUint(6),
      BaseFieldElement::FromUint(7), BaseFieldElement::FromUint(8),
  };

  // These values were computed using the public code for the STARK-friendly hash challenge at
  // https://starkware.co/hash-challenge-implementation-reference-code/#marvellous .
  // They were validated against the implementation at src/starkware/air/rescue/rescue_hash.py.
  EXPECT_EQ(
      ZiggyAir::PublicInputFromPrivateInput(secret_preimage),
      ZiggyAir::PublicKeyT({
          BaseFieldElement::FromUint(1701009513277077950),
          BaseFieldElement::FromUint(394821372906024995),
          BaseFieldElement::FromUint(428352609193758013),
          BaseFieldElement::FromUint(1822402221604548281),
      }));
}

/*
  This test builds a trace for the Ziggy AIR. Then, it computes its low degree extension to an
  evaluation domain of size 256, applies the constraints, and finally checks that the result (the
  evaluations of the composition polynomial at the evaluation domain) is of degree (32 * 4 - 1).
*/
TEST(ZiggyAir, Correctness) {
  Prng prng;

  // Pick a random secret preimage.
  ZiggyAir::SecretPreimageT secret_preimage{
      BaseFieldElement::RandomElement(&prng), BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng), BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng), BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng), BaseFieldElement::RandomElement(&prng)};

  // Compute the public key.
  const ZiggyAir::PublicKeyT public_key =
      ZiggyAir::PublicInputFromPrivateInput(secret_preimage);

  ZiggyAir ziggy_air(public_key, /*is_zero_knowledge=*/true, /*n_queries=*/10);

  Trace trace = ziggy_air.GetTrace(secret_preimage, &prng);

  {
    ZiggyAir::SecretPreimageT wrong_secret_preimage = secret_preimage;
    auto random_element_index =
        prng.template UniformInt<size_t>(0, ZiggyAir::kSecretPreimageSize - 1);
    wrong_secret_preimage[random_element_index] += BaseFieldElement::One();
    EXPECT_ASSERT(
        ziggy_air.GetTrace(wrong_secret_preimage, &prng),
        HasSubstr("Given secret preimage is not a correct preimage"));
  }

  const std::vector<ExtensionFieldElement> random_coefficients =
      prng.RandomFieldElementVector<ExtensionFieldElement>(ziggy_air.NumRandomCoefficients());

  const uint64_t expected_degree = ziggy_air.GetCompositionPolynomialDegreeBound() - 1;
  const uint64_t actual_degree =
      ComputeCompositionDegree(ziggy_air, trace, random_coefficients);
  EXPECT_EQ(actual_degree, expected_degree);
}

TEST(ZiggyAir, ConstraintsEval) {
  Prng prng;

  // Pick a random secret preimage.
  ZiggyAir::SecretPreimageT secret_preimage{
      BaseFieldElement::RandomElement(&prng), BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng), BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng), BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng), BaseFieldElement::RandomElement(&prng)};

  // Compute the public key.
  const ZiggyAir::PublicKeyT public_key =
      ZiggyAir::PublicInputFromPrivateInput(secret_preimage);
  ZiggyAir ziggy_air(public_key, /*is_zero_knowledge=*/true, /*n_queries=*/30);

  auto neighbors = prng.RandomFieldElementVector<BaseFieldElement>(2 * ZiggyAir::kStateSize);
  auto periodic_columns =
      prng.RandomFieldElementVector<BaseFieldElement>(ZiggyAir::kNumPeriodicColumns);
  auto random_coefficients =
      prng.RandomFieldElementVector<ExtensionFieldElement>(ziggy_air.NumRandomCoefficients());
  auto point_powers = prng.RandomFieldElementVector<BaseFieldElement>(3);
  auto shifts = prng.RandomFieldElementVector<BaseFieldElement>(10);

  ExtensionFieldElement eval_as_base = ziggy_air.ConstraintsEval<BaseFieldElement>(
      neighbors, {}, periodic_columns, random_coefficients, point_powers, shifts);
  ExtensionFieldElement eval_as_extension = ziggy_air.ConstraintsEval<ExtensionFieldElement>(
      std::vector<ExtensionFieldElement>{neighbors.begin(), neighbors.end()}, {},
      std::vector<ExtensionFieldElement>{periodic_columns.begin(), periodic_columns.end()},
      random_coefficients,
      std::vector<ExtensionFieldElement>{point_powers.begin(), point_powers.end()}, shifts);
  EXPECT_EQ(eval_as_base, eval_as_extension);
}

}  // namespace
}  // namespace starkware
