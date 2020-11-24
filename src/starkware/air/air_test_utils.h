#ifndef STARKWARE_AIR_AIR_TEST_UTILS_H_
#define STARKWARE_AIR_AIR_TEST_UTILS_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "starkware/air/air.h"
#include "starkware/air/trace.h"

namespace starkware {

/*
  A basic and flexible AIR class, used for testing.
*/
class DummyAir : public Air {
 public:
  explicit DummyAir(uint64_t trace_length) : Air(trace_length, /*slackness_factor=*/1) {}

  uint64_t GetCompositionPolynomialDegreeBound() const override { return 2 * TraceLength(); }

  uint64_t NumRandomCoefficients() const override { return 2 * constraints.size(); }

  uint64_t NumColumns() const override { return n_columns; }

  template <typename FieldElementT>
  ExtensionFieldElement ConstraintsEval(
      gsl::span<const FieldElementT> neighbors,
      gsl::span<const ExtensionFieldElement> composition_neighbors,
      gsl::span<const FieldElementT> periodic_columns,
      gsl::span<const ExtensionFieldElement> random_coefficients,
      gsl::span<const FieldElementT> point_powers, gsl::span<const BaseFieldElement> shifts) const {
    ASSERT_RELEASE(
        random_coefficients.size() == NumRandomCoefficients(),
        "Wrong number of random coefficients.");
    ExtensionFieldElement res = ExtensionFieldElement::Zero();
    for (const auto& constraint : constraints) {
      res += constraint(
          std::vector<ExtensionFieldElement>{neighbors.begin(), neighbors.end()},
          std::vector<ExtensionFieldElement>{composition_neighbors.begin(),
                                             composition_neighbors.end()},
          std::vector<ExtensionFieldElement>{periodic_columns.begin(), periodic_columns.end()},
          random_coefficients,
          std::vector<ExtensionFieldElement>{point_powers.begin(), point_powers.end()}, shifts);
    }
    return res;
  }

  std::unique_ptr<CompositionPolynomial> CreateCompositionPolynomial(
      const BaseFieldElement& trace_generator,
      gsl::span<const ExtensionFieldElement> random_coefficients) const override {
    typename CompositionPolynomialImpl<DummyAir>::Builder builder(periodic_columns.size());

    for (size_t i = 0; i < periodic_columns.size(); ++i) {
      builder.AddPeriodicColumn(periodic_columns[i], i);
    }

    return builder.BuildUniquePtr(
        UseOwned(this), trace_generator, TraceLength(), random_coefficients, point_exponents,
        BatchPow(trace_generator, gen_exponents));
  }

  /*
    A helper function for tests that do not specify a generator.
  */
  std::unique_ptr<CompositionPolynomial> CreateCompositionPolynomial(
      gsl::span<const ExtensionFieldElement> random_coefficients) const {
    return CreateCompositionPolynomial(GetSubGroupGenerator(TraceLength()), random_coefficients);
  }

  std::vector<std::pair<int64_t, uint64_t>> GetMask() const override { return mask; }

  size_t n_columns = 0;
  std::vector<std::pair<int64_t, uint64_t>> mask;

  std::vector<PeriodicColumn> periodic_columns;
  std::vector<uint64_t> point_exponents;
  std::vector<uint64_t> gen_exponents;
  std::vector<std::function<ExtensionFieldElement(
      gsl::span<const ExtensionFieldElement> neighbors,
      gsl::span<const ExtensionFieldElement> composition_neighbors,
      gsl::span<const ExtensionFieldElement> periodic_columns,
      gsl::span<const ExtensionFieldElement> random_coefficients,
      gsl::span<const ExtensionFieldElement> point_powers,
      gsl::span<const BaseFieldElement> shifts)>>
      constraints;
};

/*
  Returns the degree after applying the air constraints, given the provided random coefficients, on
  the provided trace. Used for air-constraints unit testing. This function assumes the random
  coefficients are used only to bind constraints together, meaning, the number of constraints is
  exactly half the number of random coefficients and the composition polynomial is of the form:
  \sum constraint_i(x) * (coeff_{2*i} + coeff_{2*i+1} * x^{n_i}).
*/
int64_t ComputeCompositionDegree(
    const Air& air, const Trace& trace, gsl::span<const ExtensionFieldElement> random_coefficients,
    size_t num_of_cosets = 2);

}  // namespace starkware

#endif  // STARKWARE_AIR_AIR_TEST_UTILS_H_
