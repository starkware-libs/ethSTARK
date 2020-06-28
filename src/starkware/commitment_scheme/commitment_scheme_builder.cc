#include "starkware/commitment_scheme/commitment_scheme_builder.h"

#include <utility>

#include "starkware/commitment_scheme/merkle/merkle_commitment_scheme.h"
#include "starkware/commitment_scheme/table_prover_impl.h"
#include "starkware/math/math.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

PackagingCommitmentSchemeProver MakeCommitmentSchemeProver(
    size_t size_of_element, size_t n_elements_in_segment, size_t n_segments,
    ProverChannel* channel) {
  PackagingCommitmentSchemeProver commitment_scheme_prover(
      size_of_element, n_elements_in_segment, n_segments, channel,
      [n_segments, channel](size_t n_elements) {
        return std::make_unique<MerkleCommitmentSchemeProver>(n_elements, n_segments, channel);
      });

  return commitment_scheme_prover;
}

PackagingCommitmentSchemeVerifier MakeCommitmentSchemeVerifier(
    size_t size_of_element, uint64_t n_elements, VerifierChannel* channel) {
  PackagingCommitmentSchemeVerifier commitment_scheme_verifier(
      size_of_element, n_elements, channel,
      [channel](size_t n_elements) -> std::unique_ptr<CommitmentSchemeVerifier> {
        MerkleCommitmentSchemeVerifier merkle_commitment_scheme_verifier(n_elements, channel);
        return std::make_unique<MerkleCommitmentSchemeVerifier>(
            std::move(merkle_commitment_scheme_verifier));
      });

  return commitment_scheme_verifier;
}

}  // namespace starkware
