/*
  Implements an AIR for the claim:
    "There exists a sequence y_i that satisfies the recursion rule y_{i+1} = y_{i}^3 * const +
    periodic_{i}, such that y_{res_claim_index} is claimed_res where const is a public constant and
    periodic is a public periodic sequence (where public means they are known to the verifier)."

  This AIR is used as a dummy AIR for the unit tests of the STARK protocol.
  The trace has 2 columns - x, y. In the first row x_0 = w. In all the rows except the last one:
    * y_i = x_i**3.
    * x_{i+1} = const * y_i + periodic.
  After the res_claim_index-th row the last rows are the continuation of the aforementioned
  sequence.
*/

#ifndef STARKWARE_AIR_TEST_AIR_TEST_AIR_H_
#define STARKWARE_AIR_TEST_AIR_TEST_AIR_H_

#include <memory>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/air.h"
#include "starkware/air/trace.h"
#include "starkware/composition_polynomial/composition_polynomial.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

class TestAir : public Air {
 public:
  using Builder = typename CompositionPolynomialImpl<TestAir>::Builder;

  explicit TestAir(
      uint64_t trace_length, uint64_t res_claim_index, const BaseFieldElement& claimed_res,
      bool is_zero_knowledge, size_t n_queries)
      : Air(trace_length, is_zero_knowledge ? ComputeSlacknessFactor(trace_length, n_queries) : 1),
        res_claim_index_(res_claim_index),
        claimed_res_(claimed_res) {
    ASSERT_RELEASE(
        res_claim_index_ < trace_length, "res_claim_index must be smaller than trace_length.");
  }

  std::unique_ptr<CompositionPolynomial> CreateCompositionPolynomial(
      const BaseFieldElement& trace_generator,
      gsl::span<const ExtensionFieldElement> random_coefficients) const override;

  uint64_t GetCompositionPolynomialDegreeBound() const override { return 4 * TraceLength(); }

  std::vector<std::pair<int64_t, uint64_t>> GetMask() const override;

  uint64_t NumRandomCoefficients() const override { return 2 * kNumConstraints; }

  uint64_t NumColumns() const override { return 2; }

  /*
    TestAir does not use composition_neighbors in its ConstraintsEval implementation.
  */
  template <typename FieldElementT>
  ExtensionFieldElement ConstraintsEval(
      gsl::span<const FieldElementT> neighbors,
      gsl::span<const ExtensionFieldElement> composition_neighbors,
      gsl::span<const FieldElementT> periodic_columns,
      gsl::span<const ExtensionFieldElement> random_coefficients,
      gsl::span<const FieldElementT> point_powers, gsl::span<const BaseFieldElement> shifts) const;

  static constexpr uint64_t kNumConstraints = 3;
  static constexpr size_t kNumNeighbors = 3;

  /*
    Generates the trace.
    witness is the first element of the sequence.
  */
  Trace GetTrace(const BaseFieldElement& witness, Prng* prng = nullptr) const;

  /*
    Given a witness and an index, generates corresponding public input.
  */
  static BaseFieldElement PublicInputFromPrivateInput(
      const BaseFieldElement& witness, uint64_t res_claim_index);

 private:
  /*
    The index of the requested element.
  */
  const uint64_t res_claim_index_;

  /*
    The value of the requested element.
  */
  const BaseFieldElement claimed_res_;

  /*
    The periodic column values in the air.
  */
  static const std::vector<BaseFieldElement> kPeriodicValues;

  /*
    The constant in the air.
  */
  static const BaseFieldElement kConst;
};

}  // namespace starkware

#include "starkware/air/test_air/test_air.inl"

#endif  // STARKWARE_AIR_TEST_AIR_TEST_AIR_H_
