#ifndef STARKWARE_AIR_AIR_H_
#define STARKWARE_AIR_AIR_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/composition_polynomial/composition_polynomial.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

class CompositionPolynomial;

/*
  Implementations of Air should implement ConstraintsEval (which is omitted here because it cannot
  be both virtual and template).

  Evaluates the composition polynomial on a single point.
  * neighbors - values obtained from the trace low degree extension using the AIR's mask.
  * composition_neighbors - values obtained from the composition_trace low degree extension using
    the AIR's mask.
  * periodic_columns - evaluations of the periodic columns on the point.
  * random_coefficients - random coefficients given by the verifier, two for each constraint.
    One to be multiplied by the original constraint, and one to be multiplied by the degree
    adjusted constraint.
  * point_powers - powers of the point needed for the evaluation of the polynomial where
    point_powers[0] = point.
    Each value is of the form point^a, where
      a
      = degreeAdjustment(composition_degree_bound, constraint_degree, numerator_degree,
        denominator_degree)
      = composition_degree_bound - (constraint_degree + numerator_degree - denominator_degree) - 1.
  * shifts - powers of the generator needed for the evaluation of the polynomial.

  template <typename FieldElementT>
  ExtensionFieldElement ConstraintsEval(
      gsl::span<const FieldElementT> neighbors,
      gsl::span<const ExtensionFieldElement> composition_neighbors,
      gsl::span<const FieldElementT> periodic_columns,
      gsl::span<const ExtensionFieldElement> random_coefficients,
      gsl::span<const FieldElementT> point_powers, gsl::span<const BaseFieldElement> shifts) const;
*/
class Air {
 public:
  virtual ~Air() = default;

  explicit Air(uint64_t original_trace_length, size_t slackness_factor)
      : slackness_factor_(slackness_factor),
        original_trace_length_(original_trace_length),
        trace_length_(original_trace_length * slackness_factor_) {
    ASSERT_RELEASE(IsPowerOfTwo(trace_length_), "trace_length must be a power of 2.");
  }

  /*
    Creates a CompositionPolynomial object based on the given (verifier-chosen) coefficients.
  */
  virtual std::unique_ptr<CompositionPolynomial> CreateCompositionPolynomial(
      const BaseFieldElement& trace_generator,
      gsl::span<const ExtensionFieldElement> random_coefficients) const = 0;

  /*
    Returns the length of the trace.
  */
  uint64_t TraceLength() const { return trace_length_; }

  /*
    Returns the degree bound of the composition polynomial.
    This is usually trace_length_ * max_constraint_degree, where max_constraint_degree is rounded up
    to a power of 2.
  */
  virtual uint64_t GetCompositionPolynomialDegreeBound() const = 0;

  /*
    Returns the number of random coefficients that the verifier chooses.
    They are the coefficients of the linear combination of the constraints and must be random in
    order to maintain soundness.
  */
  virtual uint64_t NumRandomCoefficients() const = 0;

  /*
    Returns a list of pairs (relative row, column) that define the neighbors needed for the
    constraints. For example, (0, 2), (1, 2) refer to two consecutive cells from the third column
    (2).
  */
  virtual std::vector<std::pair<int64_t, uint64_t>> GetMask() const = 0;

  /*
    Returns the number of columns.
  */
  virtual uint64_t NumColumns() const = 0;

 protected:
  size_t ComputeSlacknessFactor(size_t original_trace_length, size_t n_queries) const {
    std::vector<size_t> deep_queries_count(NumColumns(), 0);
    for (auto [row, column] : GetMask()) {  // NOLINT: structured binding.
      deep_queries_count[column]++;
    }
    const size_t max_deep_queries_count =
        *std::max_element(deep_queries_count.begin(), deep_queries_count.end());
    const size_t modified_trace_length =
        Pow2(Log2Ceil(original_trace_length + max_deep_queries_count + n_queries));
    return SafeDiv(modified_trace_length, original_trace_length);
  }

  size_t slackness_factor_;
  uint64_t original_trace_length_;
  uint64_t trace_length_;
};

}  // namespace starkware

#endif  // STARKWARE_AIR_AIR_H_
