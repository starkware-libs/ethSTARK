/*
  Implements an AIR for the Rescue hash chain claim:
    "I know a sequence of inputs {w_i} such that H(...H(H(w_0, w_1), w_2) ..., w_n) = p",
  where H is the Rescue hash function, {w_i} are 4-tuples of field elements, and p is the public
  output of the hash (which consists of 4 field elements).

  The Rescue trace consists of 12 columns, corresponding to the 12 field elements of the state.
  The hashes are computed in batches of 3 hashes that fit into 32 rows as follows:
  Row 0:
      The state of the computation in the beginning of the first hash (8 input field elements
      and 4 zeroes).
  Rows 1 to 10:
      The state of the computation in the middle of every Rescue round of the first hash.
  Rows 11 to 20:
      The state of the computation in the middle of every Rescue round of the second hash.
  Rows 21 to 30:
      The state of the computation in the middle of every Rescue round of the third hash.
  Row 31:
      The state of the computation in the end of the third third hash. The first 4 field elements in
      this state are the output.
*/
#ifndef STARKWARE_AIR_RESCUE_RESCUE_AIR_H_
#define STARKWARE_AIR_RESCUE_RESCUE_AIR_H_

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/air.h"
#include "starkware/air/rescue/rescue_constants.h"
#include "starkware/air/trace.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"

namespace starkware {

class RescueAir : public Air {
 public:
  static constexpr size_t kWordSize = 4;
  static constexpr size_t kHashesPerBatch = 3;
  static constexpr size_t kStateSize = RescueConstants::kStateSize;
  static constexpr size_t kNumRounds = RescueConstants::kNumRounds;
  static constexpr size_t kBatchHeight = 32;
  static constexpr size_t kNumColumns = kStateSize;
  static constexpr size_t kNumPeriodicColumns = 2 * kStateSize;
  static constexpr size_t kNumConstraints = 52;

  using Builder = typename CompositionPolynomialImpl<RescueAir>::Builder;
  using WordT = std::array<BaseFieldElement, kWordSize>;
  using WitnessT = std::vector<WordT>;

  /*
    The public input for the AIR consists of:
    output - the result of the last hash, a tuple of 4 elements (p).
    chain_length - the number of hash invocations in the chain (n).
  */
  RescueAir(const WordT& output, uint64_t chain_length, bool is_zero_knowledge, size_t n_queries)
      : Air(Pow2(Log2Ceil(SafeDiv(chain_length, kHashesPerBatch) * kBatchHeight)),
            is_zero_knowledge
                ? ComputeSlacknessFactor(
                      Pow2(Log2Ceil(SafeDiv(chain_length, kHashesPerBatch) * kBatchHeight)),
                      n_queries)
                : 1),
        output_(output),
        chain_length_(chain_length) {
    ASSERT_RELEASE(
        original_trace_length_ >= SafeDiv(chain_length, kHashesPerBatch) * kBatchHeight,
        "Data coset is too small.");
  }

  std::unique_ptr<CompositionPolynomial> CreateCompositionPolynomial(
      const BaseFieldElement& trace_generator,
      gsl::span<const ExtensionFieldElement> random_coefficients) const override;

  uint64_t GetCompositionPolynomialDegreeBound() const override { return 4 * TraceLength(); }

  std::vector<std::pair<int64_t, uint64_t>> GetMask() const override;

  uint64_t NumRandomCoefficients() const override { return 2 * kNumConstraints; }

  uint64_t NumColumns() const override { return kNumColumns; }

  /*
    RescueAir does not use composition_neighbors in its ConstraintsEval implementation.
  */
  template <typename FieldElementT>
  ExtensionFieldElement ConstraintsEval(
      gsl::span<const FieldElementT> neighbors,
      gsl::span<const ExtensionFieldElement> composition_neighbors,
      gsl::span<const FieldElementT> periodic_columns,
      gsl::span<const ExtensionFieldElement> random_coefficients,
      gsl::span<const FieldElementT> point_powers, gsl::span<const BaseFieldElement> shifts) const;

  /*
    Adds periodic columns to the composition polynomial.
  */
  void BuildPeriodicColumns(Builder* builder) const;

  /*
    Generates the trace.
    witness is the sequence of 4-tuples {w_i} such that H(...H(H(w_0, w_1), w_2) ..., w_n) = p.
  */
  Trace GetTrace(const WitnessT& witness, Prng* prng = nullptr) const;

  /*
    Receives a private input/witness, which is a vector of inputs to the hash chain:
        {w_0,w_1,w_2,...,w_n}
    Returns the public input, which is the output of the hash chain:
        H(...H(H(w_0, w_1), w_2) ..., w_n).
  */
  static WordT PublicInputFromPrivateInput(const WitnessT& witness);

 private:
  const WordT output_;
  const uint64_t chain_length_;
};

}  // namespace starkware

#include "starkware/air/rescue/rescue_air.inl"

#endif  // STARKWARE_AIR_RESCUE_RESCUE_AIR_H_
