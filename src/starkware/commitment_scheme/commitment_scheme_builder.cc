#include "starkware/commitment_scheme/commitment_scheme_builder.h"

#include <utility>

#include "starkware/commitment_scheme/merkle/merkle_commitment_scheme.h"
#include "starkware/commitment_scheme/table_prover_impl.h"
#include "starkware/math/math.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

std::unique_ptr<CommitmentSchemeProver> MakeCommitmentSchemeProver(
    size_t size_of_element, size_t n_elements_in_segment, size_t n_segments, ProverChannel* channel,
    bool with_salt, Prng* prng) {
  if (with_salt) {
    ASSERT_RELEASE(prng != nullptr, "Missing prng for generating salts.");
    uint64_t n_elements = n_elements_in_segment * n_segments;
    return std::make_unique<SaltedCommitmentSchemeProver>(
        size_of_element, n_elements, n_segments, channel,
        std::make_unique<MerkleCommitmentSchemeProver>(n_elements, n_segments, channel), prng);
  }
  return std::make_unique<PackagingCommitmentSchemeProver>(
      size_of_element, n_elements_in_segment, n_segments, channel,
      [n_segments, channel](size_t n_elements) {
        return std::make_unique<MerkleCommitmentSchemeProver>(n_elements, n_segments, channel);
      });
}

std::unique_ptr<CommitmentSchemeVerifier> MakeCommitmentSchemeVerifier(
    size_t size_of_element, uint64_t n_elements, VerifierChannel* channel, bool with_salt) {
  if (with_salt) {
    return std::make_unique<SaltedCommitmentSchemeVerifier>(
        size_of_element, n_elements, channel,
        std::make_unique<MerkleCommitmentSchemeVerifier>(n_elements, channel));
  }
  return std::make_unique<PackagingCommitmentSchemeVerifier>(
      size_of_element, n_elements, channel,
      [channel](size_t n_elements) -> std::unique_ptr<CommitmentSchemeVerifier> {
        MerkleCommitmentSchemeVerifier merkle_commitment_scheme_verifier(n_elements, channel);
        return std::make_unique<MerkleCommitmentSchemeVerifier>(
            std::move(merkle_commitment_scheme_verifier));
      });
}

}  // namespace starkware
