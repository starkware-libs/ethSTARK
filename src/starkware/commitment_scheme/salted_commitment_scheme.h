#ifndef STARKWARE_COMMITMENT_SCHEME_SALTED_COMMITMENT_SCHEME_H_
#define STARKWARE_COMMITMENT_SCHEME_SALTED_COMMITMENT_SCHEME_H_

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/commitment_scheme.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

class SaltedCommitmentSchemeProver : public CommitmentSchemeProver {
 public:
  static constexpr size_t kMinSegmentBytes = 2 * Blake2s256::kDigestNumBytes;
  static constexpr size_t kSaltNumBytes = Blake2s256::kDigestNumBytes / 2;
  static constexpr size_t kSizeOfElement = Blake2s256::kDigestNumBytes;

  SaltedCommitmentSchemeProver(
      size_t size_of_element, uint64_t n_elements, size_t n_segments, ProverChannel* channel,
      std::unique_ptr<CommitmentSchemeProver> inner_commitment_scheme, Prng* prng);

  size_t NumSegments() const override;
  uint64_t SegmentLengthInElements() const override;

  void AddSegmentForCommitment(
      gsl::span<const std::byte> segment_data, size_t segment_index) override;

  /*
    Commit to data by calling commit of inner_commitment_scheme_.
  */
  void Commit() override;

  std::vector<uint64_t> StartDecommitmentPhase(const std::set<uint64_t>& queries) override;

  void Decommit(gsl::span<const std::byte> elements_data) override;

  std::array<std::byte, kSaltNumBytes> GetSalt(uint64_t index) const;

 private:
  std::vector<std::byte> HashSegment(gsl::span<const std::byte> segment_data, size_t segment_index);

  const size_t size_of_element_;
  const uint64_t n_elements_;
  const size_t n_segments_;
  ProverChannel* channel_;
  std::unique_ptr<CommitmentSchemeProver> inner_commitment_scheme_;
  Prng* prng_;
  std::set<uint64_t> queries_;
};

/*
  Verifier's corresponding code of SaltedCommitmentSchemeProver.
*/
class SaltedCommitmentSchemeVerifier : public CommitmentSchemeVerifier {
 public:
  SaltedCommitmentSchemeVerifier(
      size_t size_of_element, uint64_t n_elements, VerifierChannel* channel,
      std::unique_ptr<CommitmentSchemeVerifier> inner_commitment_scheme);

  /*
    Call ReadCommitment of inner_commitment_scheme_.
  */
  void ReadCommitment() override;

  /*
    Given elements_to_verify, verify elements using data it receives from the channel, and calls
    VerifyIntegrity of the merkle layer.
  */
  bool VerifyIntegrity(
      const std::map<uint64_t, std::vector<std::byte>>& elements_to_verify) override;

 private:
  const size_t size_of_element_;
  const uint64_t n_elements_;
  VerifierChannel* channel_;
  std::unique_ptr<CommitmentSchemeVerifier> inner_commitment_scheme_;
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_SALTED_COMMITMENT_SCHEME_H_
