/*
  Implements the Ziggy AIR which is just a preimage claim for one
  invocation of the Rescue hash function: "I know an input w such that Rescue(w) = p", where w is a
  private 8-tuple of field elements and p is the public key of the signer, which consists of 4 field
  elements.

  The AIR depends only on the public key and has no dependence on the message. Instead, the message
  is included in the initial seed which is used in the Fiat-Shamir transformation.

  The trace consists of 12 columns, corresponding to the 12 field elements of the state.
  The hash is computed in 16 rows as follows:
  Row 0:          Ignored, can hold any value.
  Rows 1 to 10:   The state of the computation in the middle of every Rescue round.
  Rows 11 to 15:  Ignored, can hold any value.
  The input and output states do not appear in the trace. Instead of having a constraint that
  computes the last half round and places the result in row 11, and another constraint that forces
  the value in row 11 to be p, we combine them to one constraint that computes the last half round
  and forces the result to be p. We treat the first half round similarly.
*/
#ifndef STARKWARE_AIR_ZIGGY_ZIGGY_AIR_H_
#define STARKWARE_AIR_ZIGGY_ZIGGY_AIR_H_

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/air.h"
#include "starkware/air/rescue/rescue_air_utils.h"
#include "starkware/air/rescue/rescue_constants.h"
#include "starkware/air/trace.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"

namespace starkware {

class ZiggyAir : public Air {
 public:
  static constexpr size_t kWordSize = 4;
  static constexpr size_t kPublicKeySize = kWordSize;
  static constexpr size_t kSecretPreimageSize = 2 * kWordSize;
  static constexpr size_t kStateSize = RescueConstants::kStateSize;
  static constexpr size_t kNumRounds = RescueConstants::kNumRounds;
  static constexpr size_t kAirHeight = 16;
  static constexpr size_t kNumColumns = kStateSize;
  static constexpr size_t kNumPeriodicColumns = 2 * kStateSize;
  static constexpr size_t kNumConstraints = 20;

  using Builder = typename CompositionPolynomialImpl<ZiggyAir>::Builder;
  using PublicKeyT = std::array<BaseFieldElement, kPublicKeySize>;
  using SecretPreimageT = std::array<BaseFieldElement, kSecretPreimageSize>;

  /*
    The public input for the AIR consists of:
    public_key - the result of the hash, a tuple of 4 elements (p).
  */
  explicit ZiggyAir(const PublicKeyT& public_key, bool is_zero_knowledge, size_t n_queries)
      : Air(kAirHeight, is_zero_knowledge ? ComputeSlacknessFactor(kAirHeight, n_queries) : 1),
        public_key_(public_key) {}

  std::unique_ptr<CompositionPolynomial> CreateCompositionPolynomial(
      const BaseFieldElement& trace_generator,
      gsl::span<const ExtensionFieldElement> random_coefficients) const override;

  uint64_t GetCompositionPolynomialDegreeBound() const override { return 4 * TraceLength(); }

  std::vector<std::pair<int64_t, uint64_t>> GetMask() const override;

  uint64_t NumRandomCoefficients() const override { return 2 * kNumConstraints; }

  uint64_t NumColumns() const override { return kNumColumns; }

  /*
    ZiggyAir does not use composition_neighbors in its ConstraintsEval implementation.
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
    secret_preimage is the 8-tuple w such that Rescue(w) = p.
  */
  Trace GetTrace(const SecretPreimageT& secret_preimage, Prng* prng) const;

  /*
    Receives a secret preimage and computes the public key.
  */
  static PublicKeyT PublicInputFromPrivateInput(const SecretPreimageT& secret_preimage);

 private:
  const PublicKeyT public_key_;
};

}  // namespace starkware

#include "starkware/air/ziggy/ziggy_air.inl"

#endif  // STARKWARE_AIR_ZIGGY_ZIGGY_AIR_H_
