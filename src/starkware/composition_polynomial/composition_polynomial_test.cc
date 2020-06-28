#include "starkware/composition_polynomial/composition_polynomial.h"

#include <optional>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/air.h"
#include "starkware/air/air_test_utils.h"
#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::HasSubstr;

constexpr uint64_t kDefaultGroupOrder = 4;

TEST(CompositionPolynomial, ZeroConstraints) {
  Prng prng;

  DummyAir air(kDefaultGroupOrder);
  const std::unique_ptr<CompositionPolynomial> poly =
      air.CreateCompositionPolynomial(GetSubGroupGenerator(kDefaultGroupOrder), {});
  const ExtensionFieldElement evaluation_point = ExtensionFieldElement::RandomElement(&prng);

  EXPECT_EQ(ExtensionFieldElement::Zero(), poly->EvalAtPoint(evaluation_point, {}, {}));
}

TEST(CompositionPolynomial, WrongNumCoefficients) {
  Prng prng;
  const size_t n_coefficients = prng.UniformInt(1, 100);
  const auto coefficients = prng.RandomFieldElementVector<ExtensionFieldElement>(n_coefficients);
  DummyAir air(kDefaultGroupOrder);

  // DummyAir has no constraints, thus expecting zero coefficients.
  EXPECT_ASSERT(
      air.CreateCompositionPolynomial(coefficients), HasSubstr("Wrong number of coefficients."));
}

// A helper function to create a random periodic column.
PeriodicColumn RandomPeriodicColumn(
    Prng* prng, const std::optional<size_t>& log_coset_size_opt = std::nullopt) {
  const size_t log_coset_size = log_coset_size_opt ? *log_coset_size_opt : prng->UniformInt(0, 4);
  const auto log_n_values = prng->template UniformInt<size_t>(0, log_coset_size);

  return PeriodicColumn(
      prng->RandomFieldElementVector<BaseFieldElement>(Pow2(log_n_values)), Pow2(log_coset_size));
}

void TestEvalCompositionOnCoset(const size_t log_coset_size, const uint64_t task_size) {
  Prng prng;

  // Construct an AIR with a periodic column and a single constraint over two columns - x,y that
  // verifies: x_i * periodic_value = y_i.
  const size_t trace_length = Pow2(log_coset_size);
  DummyAir air(trace_length);
  air.periodic_columns.push_back(RandomPeriodicColumn(&prng, log_coset_size));
  air.n_columns = 2;
  air.mask = {{{0, 0}, {0, 1}}};

  const uint64_t constraint_degrees = 2 * trace_length - 2;

  air.point_exponents = {trace_length,  // Used to compute everywhere.
                         (air.GetCompositionPolynomialDegreeBound() - 1) -
                             (constraint_degrees +
                              /* nowhere */ 0 - /* everywhere */ trace_length)};
  air.constraints = {[](gsl::span<const ExtensionFieldElement> neighbors,
                        gsl::span<const ExtensionFieldElement> composition_neighbors,
                        gsl::span<const ExtensionFieldElement> periodic_columns,
                        gsl::span<const ExtensionFieldElement> random_coefficients,
                        gsl::span<const ExtensionFieldElement> point_powers,
                        gsl::span<const BaseFieldElement> /*shifts*/) {
    const ExtensionFieldElement constraint =
        (neighbors[0] * periodic_columns[0]) - composition_neighbors[0];
    const ExtensionFieldElement deg_adj =
        random_coefficients[0] + random_coefficients[1] * point_powers[2];

    // Nowhere.
    const ExtensionFieldElement& numerator = ExtensionFieldElement::One();

    // Everywhere (all rows): point^trace_length - 1.
    const ExtensionFieldElement denominator = point_powers[1] - ExtensionFieldElement::One();

    return constraint * deg_adj * numerator / denominator;
  }};

  // Create a composition polynomial.
  const BaseFieldElement coset_group_generator = GetSubGroupGenerator(trace_length);

  const auto coefficients =
      prng.RandomFieldElementVector<ExtensionFieldElement>(air.NumRandomCoefficients());

  const std::unique_ptr<CompositionPolynomial> poly =
      air.CreateCompositionPolynomial(coset_group_generator, coefficients);

  // Construct random traces.
  std::vector<std::vector<BaseFieldElement>> trace;
  trace.emplace_back(prng.RandomFieldElementVector<BaseFieldElement>(trace_length));

  std::vector<std::vector<ExtensionFieldElement>> composition_trace;
  composition_trace.emplace_back(
      prng.RandomFieldElementVector<ExtensionFieldElement>(trace_length));

  // Evaluate on a random coset.
  const BaseFieldElement coset_offset = BaseFieldElement::RandomElement(&prng);
  auto evaluation = ExtensionFieldElement::UninitializedVector(trace_length);
  poly->EvalOnCosetBitReversedOutput(
      coset_offset, std::vector<gsl::span<const BaseFieldElement>>(trace.begin(), trace.end()),
      std::vector<gsl::span<const ExtensionFieldElement>>(
          composition_trace.begin(), composition_trace.end()),
      evaluation, task_size);

  for (size_t i = 0; i < trace_length; ++i) {
    // Collect neighbors.
    std::vector<BaseFieldElement> neighbors = {trace[0][i]};
    std::vector<ExtensionFieldElement> composition_neighbors = {composition_trace[0][i]};

    // Evaluate and test.
    const BaseFieldElement curr_point = coset_offset * Pow(coset_group_generator, i);
    ASSERT_EQ(
        poly->EvalAtPoint(curr_point, neighbors, composition_neighbors),
        evaluation.at(BitReverse(i, log_coset_size)));
  }
}

TEST(CompositionPolynomial, EvalCompositionOnCoset) {
  Prng prng;
  TestEvalCompositionOnCoset(prng.UniformInt(5, 8), Pow2(4));

  // task_size is not a power of 2.
  TestEvalCompositionOnCoset(prng.UniformInt(5, 8), 20);
  // task_size > coset_size.
  TestEvalCompositionOnCoset(4, Pow2(5));
}

}  // namespace
}  // namespace starkware
