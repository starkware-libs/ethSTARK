#ifndef STARKWARE_COMMITMENT_SCHEME_MERKLE_MERKLE_COMMITMENT_SCHEME_H_
#define STARKWARE_COMMITMENT_SCHEME_MERKLE_MERKLE_COMMITMENT_SCHEME_H_

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/commitment_scheme.h"
#include "starkware/commitment_scheme/merkle/merkle.h"
#include "starkware/crypt_tools/blake2s_256.h"

namespace starkware {

class MerkleCommitmentSchemeProver : public CommitmentSchemeProver {
 public:
  static constexpr size_t kMinSegmentBytes = 2 * Blake2s256::kDigestNumBytes;
  static constexpr size_t kSizeOfElement = Blake2s256::kDigestNumBytes;

  MerkleCommitmentSchemeProver(size_t n_elements, size_t n_segments, ProverChannel* channel);

  size_t NumSegments() const override;
  uint64_t SegmentLengthInElements() const override;

  void AddSegmentForCommitment(
      gsl::span<const std::byte> segment_data, size_t segment_index) override;

  void Commit() override;

  std::vector<uint64_t> StartDecommitmentPhase(const std::set<uint64_t>& queries) override;

  void Decommit(gsl::span<const std::byte> elements_data) override;

 private:
  const uint64_t n_elements_;
  const size_t n_segments_;
  ProverChannel* channel_;
  MerkleTree tree_;
  std::set<uint64_t> queries_;
};

class MerkleCommitmentSchemeVerifier : public CommitmentSchemeVerifier {
 public:
  MerkleCommitmentSchemeVerifier(uint64_t n_elements, VerifierChannel* channel);

  void ReadCommitment() override;
  bool VerifyIntegrity(
      const std::map<uint64_t, std::vector<std::byte>>& elements_to_verify) override;

 private:
  uint64_t n_elements_;
  VerifierChannel* channel_;
  std::optional<Blake2s256> commitment_;
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_MERKLE_MERKLE_COMMITMENT_SCHEME_H_
