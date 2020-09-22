#include "starkware/commitment_scheme/merkle/merkle_commitment_scheme.h"

#include <algorithm>
#include <string>

#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {

MerkleCommitmentSchemeProver::MerkleCommitmentSchemeProver(
    size_t n_elements, size_t n_segments, ProverChannel* channel)
    : n_elements_(n_elements),
      n_segments_(n_segments),
      channel_(channel),
      // Initialize the tree with the number of elements, each element is the hash stored in a leaf.
      tree_(MerkleTree(n_elements_)) {}

size_t MerkleCommitmentSchemeProver::NumSegments() const { return n_segments_; }

uint64_t MerkleCommitmentSchemeProver::SegmentLengthInElements() const {
  return SafeDiv(n_elements_, n_segments_);
}

void MerkleCommitmentSchemeProver::AddSegmentForCommitment(
    gsl::span<const std::byte> segment_data, size_t segment_index) {
  ASSERT_RELEASE(
      segment_data.size() == SegmentLengthInElements() * kSizeOfElement,
      "Segment size is " + std::to_string(segment_data.size()) + " instead of the expected " +
          std::to_string(kSizeOfElement * SegmentLengthInElements()) + ".");
  ASSERT_RELEASE(segment_index < n_segments_, "segment_index must be smaller than n_segments_.");
  tree_.AddData(
      segment_data.as_span<const Blake2s256>(), segment_index * SegmentLengthInElements());
}

void MerkleCommitmentSchemeProver::Commit() {
  // After adding all segments, all inner tree nodes that are at least (tree_height -
  // log2(n_elements_in_segment_)) far from the root - were already computed.
  size_t tree_height = SafeLog2(tree_.GetDataLength());
  const Blake2s256 commitment = tree_.GetRoot(tree_height - SafeLog2(SegmentLengthInElements()));
  channel_->SendCommitmentHash(commitment, "Commitment");
}

std::vector<uint64_t> MerkleCommitmentSchemeProver::StartDecommitmentPhase(
    const std::set<uint64_t>& queries) {
  queries_ = queries;
  return {};
}

void MerkleCommitmentSchemeProver::Decommit(gsl::span<const std::byte> elements_data) {
  ASSERT_RELEASE(elements_data.empty(), "element_data is expected to be empty.");
  tree_.GenerateDecommitment(queries_, channel_);
}

MerkleCommitmentSchemeVerifier::MerkleCommitmentSchemeVerifier(
    uint64_t n_elements, VerifierChannel* channel)
    : n_elements_(n_elements), channel_(channel) {}

void MerkleCommitmentSchemeVerifier::ReadCommitment() {
  commitment_ = channel_->ReceiveCommitmentHash("Commitment");
}

bool MerkleCommitmentSchemeVerifier::VerifyIntegrity(
    const std::map<uint64_t, std::vector<std::byte>>& elements_to_verify) {
  // Convert data to hashes.
  std::map<uint64_t, Blake2s256> hashes_to_verify;

  for (auto const& element : elements_to_verify) {
    ASSERT_RELEASE(element.first < n_elements_, "Query out of range.");
    ASSERT_RELEASE(
        element.second.size() == Blake2s256::kDigestNumBytes, "Element size mismatches.");
    hashes_to_verify[element.first] = Blake2s256::InitDigestTo(element.second);
  }
  // Verify decommitment.
  return MerkleTree::VerifyDecommitment(hashes_to_verify, n_elements_, *commitment_, channel_);
}

}  // namespace starkware
