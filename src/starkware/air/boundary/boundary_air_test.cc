#include "starkware/air/boundary/boundary_air.h"

#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/air_test_utils.h"
#include "starkware/algebra/domains/evaluation_domain.h"
#include "starkware/algebra/lde/lde_manager.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/composition_polynomial/composition_polynomial.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/math/math.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {
namespace {

/*
  This test builds a random trace. It then generate random points to sample on random columns, and
  creates boundary constraints for them. Finally, it tests that the resulting composition polynomial
  is indeed of the expected (low) degree, which means the constraints are correctly enforced.
*/
TEST(BoundaryAir, Correctness) {
  Prng prng;

  const size_t n_columns = 10;
  const size_t n_conditions = 20;
  const uint64_t trace_length = 1024;
  std::vector<std::vector<BaseFieldElement>> trace;

  const Coset domain_coset(trace_length, BaseFieldElement::One());

  auto lde_manager = MakeLdeManager<BaseFieldElement>(domain_coset);

  // Generate random trace.
  trace.reserve(n_columns);
  for (size_t i = 0; i < n_columns; ++i) {
    trace.push_back(prng.RandomFieldElementVector<BaseFieldElement>(trace_length));
    lde_manager->AddEvaluation(trace[i]);
  }

  // Compute correct boundary conditions.
  bool add_zero_knowledge = prng.UniformInt<size_t>(0, 1) == 1;
  const int64_t num_unconstrained = add_zero_knowledge ? 1 : 0;
  std::vector<std::tuple<size_t, ExtensionFieldElement, ExtensionFieldElement>> boundary_conditions;
  for (size_t condition_index = 0; condition_index < n_conditions; ++condition_index) {
    const size_t column_index = prng.UniformInt<size_t>(0, n_columns - num_unconstrained - 1);
    auto points_x = prng.RandomFieldElementVector<ExtensionFieldElement>(1);
    auto points_y = ExtensionFieldElement::UninitializedVector(1);

    lde_manager->EvalAtPoints<ExtensionFieldElement>(column_index, points_x, points_y);
    boundary_conditions.emplace_back(column_index, points_x[0], points_y[0]);
  }

  BoundaryAir air(
      trace_length, n_columns, boundary_conditions,
      add_zero_knowledge ? std::optional<size_t>(n_columns - 1) : std::nullopt);

  const std::vector<ExtensionFieldElement> random_coefficients =
      prng.RandomFieldElementVector<ExtensionFieldElement>(air.NumRandomCoefficients());

  const uint64_t actual_degree =
      ComputeCompositionDegree(air, Trace(std::move(trace)), random_coefficients);

  // If all columns are constrained, then degree is expected to be trace_length - 2 (and not
  // trace_length - 1) as we do not apply degree adjustments.
  EXPECT_EQ(actual_degree, add_zero_knowledge ? trace_length - 1 : trace_length - 2);
  EXPECT_EQ(air.GetCompositionPolynomialDegreeBound(), trace_length);
}

/*
  Similar to above test, only the boundary conditions are deliberately wrong, meaning the resulting
  composition polynomial should be of high degree.
*/
TEST(BoundaryAir, Soundeness) {
  Prng prng;

  const size_t n_columns = 10;
  const size_t n_conditions = 20;
  const uint64_t trace_length = 1024;
  std::vector<std::vector<BaseFieldElement>> trace;

  // Generate random trace.
  trace.reserve(n_columns);
  for (size_t i = 0; i < n_columns; ++i) {
    trace.push_back(prng.RandomFieldElementVector<BaseFieldElement>(trace_length));
  }

  // Compute incorrect boundary conditions.
  bool add_zero_knowledge = prng.UniformInt<size_t>(0, 1) == 1;
  const int64_t num_unconstrained = add_zero_knowledge ? 1 : 0;
  std::vector<std::tuple<size_t, ExtensionFieldElement, ExtensionFieldElement>> boundary_conditions;
  for (size_t condition_index = 0; condition_index < n_conditions; ++condition_index) {
    const size_t column_index = prng.UniformInt<size_t>(0, n_columns - num_unconstrained - 1);
    boundary_conditions.emplace_back(
        column_index, ExtensionFieldElement::RandomElement(&prng),
        ExtensionFieldElement::RandomElement(&prng));
  }

  BoundaryAir air(
      trace_length, n_columns, boundary_conditions,
      add_zero_knowledge ? std::optional<size_t>(n_columns - 1) : std::nullopt);

  const std::vector<ExtensionFieldElement> random_coefficients =
      prng.RandomFieldElementVector<ExtensionFieldElement>(air.NumRandomCoefficients());

  const size_t num_of_cosets = 2;
  const uint64_t actual_degree =
      ComputeCompositionDegree(air, Trace(std::move(trace)), random_coefficients, num_of_cosets);

  EXPECT_EQ(num_of_cosets * air.GetCompositionPolynomialDegreeBound() - 1, actual_degree);
}

}  // namespace
}  // namespace starkware
