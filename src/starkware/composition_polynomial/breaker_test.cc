#include "starkware/composition_polynomial/breaker.h"

#include <optional>
#include <utility>

#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/lde/lde_manager.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

/*
  1. Take a random evaluation f(x).
  2. Break to 2^log_breaks parts using PolynomialBreak:Break(). Each part is the evaluation of
     h_i(x).
  3. Pick random points x_j. Compute f(x_j), and h_i(x_j^(2^log_breaks)) using LdeManagers.
  4. Use PolynomialBreak::EvalFromSamples() to compute f(x_j) and compare to the result from the
     previous step.
*/
void TestPolynomialBreak(
    const size_t log_domain, const size_t log_breaks, const size_t n_check_points) {
  Prng prng;
  const size_t domain_size = Pow2(log_domain);
  const size_t n_breaks = Pow2(log_breaks);
  const BaseFieldElement offset = BaseFieldElement::RandomElement(&prng);
  const Coset coset = Coset(domain_size, offset);
  const Coset broken_coset = Coset(SafeDiv(domain_size, n_breaks), Pow(offset, n_breaks));
  auto poly_break = PolynomialBreak(coset, log_breaks);

  // Step 1. Take a random evaluation f(x).
  auto evaluation = prng.RandomFieldElementVector<ExtensionFieldElement>(domain_size);

  // Step 2. Break.
  std::vector<ExtensionFieldElement> break_storage(
      evaluation.size(), ExtensionFieldElement::Uninitialized());
  auto broken_evals = poly_break.Break(evaluation, break_storage);

  // Step 3.
  // Pick random points x_j.
  const auto points = prng.RandomFieldElementVector<ExtensionFieldElement>(n_check_points);

  // Compute f(x_j).
  LdeManager<ExtensionFieldElement> lde_manager(coset, /*eval_in_natural_order=*/false);
  lde_manager.AddEvaluation(evaluation);

  std::vector<ExtensionFieldElement> expected_results(
      n_check_points, ExtensionFieldElement::Uninitialized());
  lde_manager.EvalAtPoints<ExtensionFieldElement>(0, points, expected_results);

  // Prepare to compute h_i(x_j^(2^log_breaks)).
  LdeManager<ExtensionFieldElement> broken_lde_manager(
      broken_coset, /*eval_in_natural_order=*/false);

  for (const auto& broken_eval : broken_evals) {
    broken_lde_manager.AddEvaluation(broken_eval);
  }

  // Compute x_j^(2^log_breaks).
  std::vector<ExtensionFieldElement> points_transformed;
  points_transformed.reserve(n_check_points);
  for (const ExtensionFieldElement& point : points) {
    points_transformed.emplace_back(Pow(point, n_breaks));
  }

  // Compute h_i(x_j^(2^log_breaks)).
  std::vector<std::vector<ExtensionFieldElement>> sub_results;
  sub_results.reserve(n_breaks);
  for (size_t i = 0; i < n_breaks; ++i) {
    sub_results.emplace_back(n_check_points, ExtensionFieldElement::Uninitialized());
    broken_lde_manager.EvalAtPoints<ExtensionFieldElement>(
        i, points_transformed, sub_results.back());
  }

  // Step 4.
  std::vector<ExtensionFieldElement> samples(n_breaks, ExtensionFieldElement::Uninitialized());
  for (size_t j = 0; j < n_check_points; ++j) {
    for (size_t i = 0; i < n_breaks; ++i) {
      samples[i] = sub_results[i][j];
    }
    // Use PolynomialBreaker::EvalFromSamples() to compute f(x_j).
    ExtensionFieldElement actual_result = poly_break.EvalFromSamples(samples, points[j]);

    // Compare to the result from the previous step.
    EXPECT_EQ(actual_result, expected_results[j]);
  }
}

TEST(PolynomialBreak, Basic) {
  TestPolynomialBreak(5, 3, 10);
  TestPolynomialBreak(5, 5, 10);
}

}  // namespace
}  // namespace starkware
