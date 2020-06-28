#ifndef STARKWARE_COMMITMENT_SCHEME_COMMITMENT_SCHEME_MOCK_H_
#define STARKWARE_COMMITMENT_SCHEME_COMMITMENT_SCHEME_MOCK_H_

#include <cstddef>
#include <map>
#include <set>
#include <vector>

#include "gmock/gmock.h"
#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/commitment_scheme/commitment_scheme.h"

namespace starkware {

class CommitmentSchemeProverMock : public CommitmentSchemeProver {
 public:
  MOCK_CONST_METHOD0(NumSegments, size_t());
  MOCK_CONST_METHOD0(SegmentLengthInElements, uint64_t());
  MOCK_METHOD2(
      AddSegmentForCommitment, void(gsl::span<const std::byte> segment_data, size_t segment_index));
  MOCK_METHOD0(Commit, void());
  MOCK_METHOD1(StartDecommitmentPhase, std::vector<uint64_t>(const std::set<uint64_t>& queries));
  MOCK_METHOD1(Decommit, void(gsl::span<const std::byte> elements_data));
};

class CommitmentSchemeVerifierMock : public CommitmentSchemeVerifier {
 public:
  MOCK_METHOD0(ReadCommitment, void());
  MOCK_METHOD1(
      VerifyIntegrity, bool(const std::map<uint64_t, std::vector<std::byte>>& elements_to_verify));
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_COMMITMENT_SCHEME_MOCK_H_
