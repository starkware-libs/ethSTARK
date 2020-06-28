#include "starkware/air/air_test_utils.h"

#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

TEST(AirTestUtils, ComputeCompositionDegree) {
  // Initialize test parameters.
  Prng prng;

  // Construct a simple AIR implementation, over two columns with a single constraint verifying that
  // the second column is a square of the first column over the data domain.
  const size_t trace_length = Pow2(5);
  DummyAir air(trace_length);
  air.n_columns = 2;
  air.mask = {{{0, 0}, {0, 1}}};

  uint64_t constraint_degrees = 2 * trace_length - 2;

  air.point_exponents = {trace_length,  // Used to compute everywhere.
                         (air.GetCompositionPolynomialDegreeBound() - 1) -
                             (constraint_degrees +
                              /* nowhere */ 0 - /* everywhere */ trace_length)};
  air.constraints = {[](gsl::span<const ExtensionFieldElement> neighbors,
                        gsl::span<const ExtensionFieldElement> /*composition_neighbors*/,
                        gsl::span<const ExtensionFieldElement> /*periodic_columns*/,
                        gsl::span<const ExtensionFieldElement> random_coefficients,
                        gsl::span<const ExtensionFieldElement> point_powers,
                        gsl::span<const BaseFieldElement> /*shifts*/) {
    const ExtensionFieldElement constraint = (neighbors[0] * neighbors[0]) - neighbors[1];
    const ExtensionFieldElement deg_adj =
        random_coefficients[0] + random_coefficients[1] * point_powers[2];

    // Everywhere (all rows): point^trace_length - 1.
    const ExtensionFieldElement denominator = point_powers[1] - ExtensionFieldElement::One();

    return constraint * deg_adj / denominator;
  }};

  // Construct trace columns.
  const std::vector<BaseFieldElement> v_rand1 =
      prng.RandomFieldElementVector<BaseFieldElement>(trace_length);
  std::vector<BaseFieldElement> v_rand1_sqr;
  v_rand1_sqr.reserve(v_rand1.size());
  for (const auto& e : v_rand1) {
    v_rand1_sqr.push_back(e * e);
  }
  const std::vector<BaseFieldElement> v_rand2 =
      prng.RandomFieldElementVector<BaseFieldElement>(trace_length);

  // Draw random coefficients.
  const std::vector<ExtensionFieldElement> random_coefficients =
      prng.RandomFieldElementVector<ExtensionFieldElement>(air.NumRandomCoefficients());

  // Construct traces.
  Trace good_trace(std::vector<std::vector<BaseFieldElement>>({v_rand1, v_rand1_sqr}));
  Trace bad_trace(std::vector<std::vector<BaseFieldElement>>({v_rand1, v_rand2}));

  // Verify degrees.
  EXPECT_EQ(
      ComputeCompositionDegree(air, good_trace, random_coefficients),
      air.GetCompositionPolynomialDegreeBound() - 1);
  EXPECT_EQ(
      ComputeCompositionDegree(air, bad_trace, random_coefficients),
      2 * air.GetCompositionPolynomialDegreeBound() - 1);
}

}  // namespace
}  // namespace starkware
