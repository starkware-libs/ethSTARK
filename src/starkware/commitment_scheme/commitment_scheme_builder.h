#ifndef STARKWARE_COMMITMENT_SCHEME_COMMITMENT_SCHEME_BUILDER_H_
#define STARKWARE_COMMITMENT_SCHEME_COMMITMENT_SCHEME_BUILDER_H_

#include <memory>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/prover_channel.h"
#include "starkware/commitment_scheme/commitment_scheme.h"
#include "starkware/commitment_scheme/packaging_commitment_scheme.h"
#include "starkware/commitment_scheme/salted_commitment_scheme.h"

namespace starkware {

/*
  Creates a commitment scheme prover.
*/
std::unique_ptr<CommitmentSchemeProver> MakeCommitmentSchemeProver(
    size_t size_of_element, size_t n_elements_in_segment, size_t n_segments, ProverChannel* channel,
    bool with_salt = false, Prng* prng = nullptr);

/*
  Creates a commitment scheme verifier.
*/
std::unique_ptr<CommitmentSchemeVerifier> MakeCommitmentSchemeVerifier(
    size_t size_of_element, uint64_t n_elements, VerifierChannel* channel, bool with_salt = false);

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_COMMITMENT_SCHEME_BUILDER_H_
