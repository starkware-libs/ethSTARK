#include "starkware/fri/fri_details.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/lde/lde_manager.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/fri/fri_folder.h"
#include "starkware/fri/fri_test_utils.h"

namespace starkware {
namespace fri {
namespace details {
namespace {

using testing::ElementsAreArray;

ExtensionFieldElement GetCorrectionFactor([[maybe_unused]] size_t n_layers) {
  return Pow(ExtensionFieldElement::FromUint(2), n_layers);
}

int64_t GetEvaluationDegree(
    std::vector<ExtensionFieldElement> vec, const Coset& source_domain_coset) {
  auto lde_manager = MakeLdeManager<ExtensionFieldElement>(source_domain_coset, false);
  lde_manager->AddEvaluation(std::move(vec));

  return lde_manager->GetEvaluationDegree(0);
}

TEST(FriDetailsTest, ComputeNextFriLayerBasicTest) {
  Prng prng;

  const size_t log_domain_size = 5;
  const Coset eval_domain(Pow2(log_domain_size), BaseFieldElement::RandomElement(&prng));
  const Coset next_layer_domain = GetCosetForFriLayer(eval_domain, 1);

  const auto eval_point = ExtensionFieldElement::RandomElement(&prng);

  // Start with a random polynomial evaluated at a domain of 16 points.
  const auto coefs = prng.RandomFieldElementVector<ExtensionFieldElement>(4);

  std::vector<ExtensionFieldElement> first_layer_eval;
  for (const BaseFieldElement& x :
       eval_domain.GetElements(MultiplicativeGroupOrdering::kBitReversedOrder)) {
    // a_3*x^3 + a_2*x^2 + a_1*x + a_0.
    first_layer_eval.push_back(coefs[0] + coefs[1] * x + coefs[2] * x * x + coefs[3] * x * x * x);
  }

  // The expected result is the polynomial 2 * ((a_2 * x + a_0) + eval_point * (a_3 * x + a_1)).
  std::vector<ExtensionFieldElement> expected_res;
  for (const BaseFieldElement& x :
       next_layer_domain.GetElements(MultiplicativeGroupOrdering::kBitReversedOrder)) {
    expected_res.push_back(
        GetCorrectionFactor(1) *
        ((coefs[2] * x + coefs[0]) + eval_point * (coefs[3] * x + coefs[1])));
  }

  const auto res = FriFolder::ComputeNextFriLayer(eval_domain, first_layer_eval, eval_point);
  EXPECT_THAT(res, ElementsAreArray(expected_res));
}

TEST(FriDetailsTest, ComputeNextFriLayerDegreeBound800) {
  Prng prng;
  const size_t log_domain_size = 13;

  const auto offset = BaseFieldElement::RandomElement(&prng);
  const auto domain = Coset(Pow2(log_domain_size), offset);
  const auto next_layer_domain = GetCosetForFriLayer(domain, 1);

  const auto eval_point = ExtensionFieldElement::RandomElement(&prng);

  // Start with a polynomial of degree bound 800 (degree 799), evaluated at a domain of 8192 points.
  const TestPolynomial test_layer(&prng, 800);

  const auto first_layer = test_layer.GetData(domain);
  EXPECT_EQ(799, GetEvaluationDegree(first_layer, domain));

  // The expected result is a polynomial of degree <400.
  const auto res = FriFolder::ComputeNextFriLayer(domain, first_layer, eval_point);

  EXPECT_EQ(399, GetEvaluationDegree(res, next_layer_domain));
}

/*
  This test checks that if the evaluation points are x0, x0^2, x0^4, x0^8, ...
  then f_i(x0^(2^i)) = f(x0) where f_i is the i-th layer in FRI.
*/
TEST(FriDetailsTest, ComputeNextFriLayerUseFriToEvaluateAtPoint) {
  Prng prng;
  const size_t log_domain_size = 13;
  const size_t degree = 16;

  const Coset eval_domain(Pow2(log_domain_size), BaseFieldElement::One());

  auto eval_point = ExtensionFieldElement::RandomElement(&prng);

  const TestPolynomial test_layer(&prng, degree + 1);
  std::vector<ExtensionFieldElement> current_layer = test_layer.GetData(eval_domain);
  const ExtensionFieldElement f_e = ExtrapolatePoint(eval_domain, current_layer, eval_point);

  for (size_t i = 1; i < log_domain_size; ++i) {
    current_layer = FriFolder::ComputeNextFriLayer(
        GetCosetForFriLayer(eval_domain, i - 1), current_layer, eval_point);
    eval_point *= eval_point;

    const ExtensionFieldElement res =
        ExtrapolatePoint(GetCosetForFriLayer(eval_domain, i), current_layer, eval_point);

    const ExtensionFieldElement correction_factor = GetCorrectionFactor(i);
    EXPECT_EQ(correction_factor * f_e, res) << "i = " << i;
  }
}

TEST(FriDetailsTest, ApplyFriLayersCorrectness) {
  Prng prng;
  const auto offset = BaseFieldElement::RandomElement(&prng);
  const size_t log2_eval_domain = 5;
  const FriParameters params{
      /*fri_step_list=*/{1, 2},
      /*last_layer_degree_bound=*/1,
      /*n_queries=*/1,
      /*domain=*/Coset(Pow2(log2_eval_domain), offset),
      /*proof_of_work_bits=*/15,
  };
  const auto eval_point = ExtensionFieldElement::RandomElement(&prng);

  // fri_step = 1.
  const std::vector<ExtensionFieldElement> elements =
      prng.RandomFieldElementVector<ExtensionFieldElement>(2);
  const size_t coset_offset = 4;
  EXPECT_EQ(
      FriFolder::NextLayerElementFromTwoPreviousLayerElements(
          elements[0], elements[1], eval_point,
          params.GetCosetForLayer(0).AtBitReversed(coset_offset)),
      ApplyFriLayers(elements, eval_point, params, 0, coset_offset));

  // fri_step = 2.
  const std::vector<ExtensionFieldElement> elements2 =
      prng.RandomFieldElementVector<ExtensionFieldElement>(4);
  const size_t coset_offset2 = 12;
  EXPECT_EQ(
      FriFolder::NextLayerElementFromTwoPreviousLayerElements(
          FriFolder::NextLayerElementFromTwoPreviousLayerElements(
              elements2[0], elements2[1], eval_point,
              params.GetCosetForLayer(1).AtBitReversed(coset_offset2)),
          FriFolder::NextLayerElementFromTwoPreviousLayerElements(
              elements2[2], elements2[3], eval_point,
              params.GetCosetForLayer(1).AtBitReversed(coset_offset2 + 2)),
          eval_point * eval_point, params.GetCosetForLayer(2).AtBitReversed(coset_offset2 / 2)),
      ApplyFriLayers(elements2, eval_point, params, 1, coset_offset2));
}

TEST(FriDetailsTest, ApplyFriLayersPolynomial) {
  const size_t fri_step = 3;
  Prng prng;
  const auto offset = BaseFieldElement::RandomElement(&prng);
  const Coset eval_domain(Pow2(fri_step), offset);
  const FriParameters params{
      /*fri_step_list=*/{fri_step},
      /*last_layer_degree_bound=*/1,
      /*n_queries=*/1,
      /*domain=*/eval_domain,
      /*proof_of_work_bits=*/15,
  };
  const auto eval_point = ExtensionFieldElement::RandomElement(&prng);

  // Take a polynomial of degree 7 evaluated at a domain of 8 points.
  const TestPolynomial test_layer(&prng, Pow2(fri_step));
  const ExtensionFieldElement res =
      ApplyFriLayers(test_layer.GetData(eval_domain), eval_point, params, 0, 0);
  const ExtensionFieldElement correction_factor = GetCorrectionFactor(fri_step);
  EXPECT_EQ(test_layer.EvalAt(eval_point) * correction_factor, res);
}

}  // namespace
}  // namespace details
}  // namespace fri
}  // namespace starkware
