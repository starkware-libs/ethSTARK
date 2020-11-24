#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/commitment_scheme_builder.h"
#include "starkware/commitment_scheme/merkle/merkle_commitment_scheme.h"
#include "starkware/commitment_scheme/packaging_commitment_scheme.h"
#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/math/math.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::HasSubstr;

struct MerkleCommitmentSchemePairT {
  using ProverT = MerkleCommitmentSchemeProver;
  using VerifierT = MerkleCommitmentSchemeVerifier;

  static ProverT CreateProver(
      ProverChannel* prover_channel, size_t /*size_of_element*/, size_t n_elements_in_segment,
      size_t n_segments, Prng* /*prng*/) {
    return MerkleCommitmentSchemeProver(
        n_elements_in_segment * n_segments, n_segments, prover_channel);
  }

  static VerifierT CreateVerifier(
      VerifierChannel* verifier_channel, size_t /*size_of_element*/, size_t n_elements) {
    return MerkleCommitmentSchemeVerifier(n_elements, verifier_channel);
  }

  static size_t DrawSizeOfElement(Prng* /*prng*/) { return Blake2s256::kDigestNumBytes; }

  static constexpr size_t kMinElementSize = Blake2s256::kDigestNumBytes;
};

struct SaltedCommitmentSchemePairT {
  using ProverT = SaltedCommitmentSchemeProver;
  using VerifierT = SaltedCommitmentSchemeVerifier;

  static ProverT CreateProver(
      ProverChannel* prover_channel, size_t size_of_element, size_t n_elements_in_segment,
      size_t n_segments, Prng* prng) {
    return SaltedCommitmentSchemeProver(
        size_of_element, n_elements_in_segment * n_segments, n_segments, prover_channel,
        std::make_unique<MerkleCommitmentSchemeProver>(
            n_elements_in_segment * n_segments, n_segments, prover_channel),
        prng);
  }

  static VerifierT CreateVerifier(
      VerifierChannel* verifier_channel, size_t size_of_element, size_t n_elements) {
    return SaltedCommitmentSchemeVerifier(
        size_of_element, n_elements, verifier_channel,
        std::make_unique<MerkleCommitmentSchemeVerifier>(n_elements, verifier_channel));
  }

  static size_t DrawSizeOfElement(Prng* prng) {
    return prng->UniformInt<size_t>(1, sizeof(Blake2s256) * 5);
  }

  static constexpr size_t kMinElementSize = Blake2s256::kDigestNumBytes;
};

struct PackagingCommitmentSchemePairT {
  using ProverT = PackagingCommitmentSchemeProver;
  using VerifierT = PackagingCommitmentSchemeVerifier;
  static ProverT CreateProver(
      ProverChannel* prover_channel, size_t size_of_element, size_t n_elements_in_segment,
      size_t n_segments, Prng* /*prng*/) {
    return PackagingCommitmentSchemeProver(
        size_of_element, n_elements_in_segment, n_segments, prover_channel,
        [n_segments, prover_channel](size_t n_elements) {
          return std::make_unique<MerkleCommitmentSchemeProver>(
              n_elements, n_segments, prover_channel);
        });
  }

  static VerifierT CreateVerifier(
      VerifierChannel* verifier_channel, size_t size_of_element, size_t n_elements) {
    return PackagingCommitmentSchemeVerifier(
        size_of_element, n_elements, verifier_channel,
        [verifier_channel](size_t n_elements) -> std::unique_ptr<CommitmentSchemeVerifier> {
          return std::make_unique<MerkleCommitmentSchemeVerifier>(n_elements, verifier_channel);
        });
  }

  static size_t DrawSizeOfElement(Prng* prng) {
    return prng->UniformInt<size_t>(1, sizeof(Blake2s256) * 5);
  }

  static constexpr size_t kMinElementSize = 1;
};

using TestTypes = ::testing::Types<
    MerkleCommitmentSchemePairT, SaltedCommitmentSchemePairT, PackagingCommitmentSchemePairT>;

/*
  Returns number of segments to use, N, such that:

  1) N is a power of 2.

  2) size_of_element * n_elements >= N * min_segment_bytes (in particular, each segment is of length
  at least min_segment_bytes).

  3) N <= n_elements.
*/
size_t DrawNumSegments(
    const size_t size_of_element, const size_t n_elements, const size_t min_segment_bytes,
    Prng* prng) {
  const size_t total_bytes = size_of_element * n_elements;
  const size_t max_n_segments =
      std::min(std::max<size_t>(1, total_bytes / min_segment_bytes), n_elements);
  const size_t max_log_n_segments = Log2Floor(max_n_segments);
  return Pow2(prng->UniformInt<size_t>(0, max_log_n_segments));
}

template <typename T>
class CommitmentScheme : public ::testing::Test {
 public:
  using CommitmentProver = typename T::ProverT;

  CommitmentScheme()
      : size_of_element_(T::DrawSizeOfElement(&prng_)),
        n_elements_(Pow2(prng_.UniformInt<size_t>(0, 10))),
        n_segments_(DrawNumSegments(
            size_of_element_, n_elements_, CommitmentProver::kMinSegmentBytes, &prng_)),
        data_(prng_.RandomByteVector(size_of_element_ * GetNumElements())),
        queries_(DrawRandomQueries()) {}

 protected:
  Prng prng_;
  const Prng channel_prng_{};
  size_t size_of_element_;
  size_t n_elements_;
  size_t n_segments_;
  std::vector<std::byte> data_;
  std::set<uint64_t> queries_;

  // Common methods.
  ProverChannel GetProverChannel() const { return ProverChannel(channel_prng_.Clone()); }

  VerifierChannel GetVerifierChannel(const gsl::span<const std::byte> proof) const {
    return VerifierChannel(channel_prng_.Clone(), proof);
  }

  std::set<uint64_t> DrawRandomQueries() {
    const auto n_queries = prng_.UniformInt<size_t>(1, 10);
    std::set<uint64_t> queries;
    for (size_t i = 0; i < n_queries; ++i) {
      queries.insert(prng_.UniformInt<uint64_t>(0, GetNumElements() - 1));
    }

    return queries;
  }

  size_t GetNumElements() const { return n_elements_; }
  size_t GetNumElementsInSegment() const { return SafeDiv(n_elements_, n_segments_); }

  gsl::span<const std::byte> GetSegment(const size_t index) const {
    const size_t n_segment_bytes = size_of_element_ * GetNumElementsInSegment();
    return gsl::make_span(data_).subspan(index * n_segment_bytes, n_segment_bytes);
  }

  std::vector<std::byte> GetElement(const size_t index) const {
    return std::vector<std::byte>(
        data_.begin() + index * size_of_element_, data_.begin() + (index + 1) * size_of_element_);
  }

  std::map<uint64_t, std::vector<std::byte>> GetElementsToVerify() const {
    std::map<uint64_t, std::vector<std::byte>> elements_to_verify;
    for (const uint64_t& q : queries_) {
      elements_to_verify[q] = GetElement(q);
    }
    return elements_to_verify;
  }

  const std::vector<std::byte> GenerateProof() {
    using CommitmentProver = typename T::ProverT;

    // Initialize prover channel.
    ProverChannel prover_channel = GetProverChannel();

    // Initialize prng.
    Prng prng;

    // Initialize the commitment-scheme prover side.
    CommitmentProver committer = T::CreateProver(
        &prover_channel, size_of_element_, GetNumElementsInSegment(), n_segments_, &prng);

    // Commit on the data_.
    for (size_t i = 0; i < n_segments_; ++i) {
      const auto segment = GetSegment(i);
      committer.AddSegmentForCommitment(segment, i);
    }
    committer.Commit();

    // Generate decommitment for queries_ already drawn.
    const std::vector<uint64_t> element_idxs = committer.StartDecommitmentPhase(queries_);
    std::vector<std::byte> elements_data;
    for (const uint64_t index : element_idxs) {
      const auto element = GetElement(index);
      std::copy(element.begin(), element.end(), std::back_inserter(elements_data));
    }
    committer.Decommit(elements_data);

    // Return proof.
    return prover_channel.GetProof();
  }

  bool VerifyProof(
      const std::vector<std::byte>& proof,
      const std::map<uint64_t, std::vector<std::byte>>& elements_to_verify) {
    using CommitmentVerifier = typename T::VerifierT;

    // Initialize verifier channel.
    VerifierChannel verifier_channel = GetVerifierChannel(proof);

    // Initialize commitment-scheme verifier.
    CommitmentVerifier verifier =
        T::CreateVerifier(&verifier_channel, size_of_element_, GetNumElements());

    // Read commitment.
    verifier.ReadCommitment();

    // Verify consistency of data_ with commitment.
    return verifier.VerifyIntegrity(elements_to_verify);
  }
};

TYPED_TEST_CASE(CommitmentScheme, TestTypes);

TYPED_TEST(CommitmentScheme, Completeness) {
  // Generate decommitment, using CommitmentSchemeProver instance.
  const auto proof = this->GenerateProof();

  // Fetch data_ in queried locations.
  std::map<uint64_t, std::vector<std::byte>> elements_to_verify = this->GetElementsToVerify();

  // Verify integrity of queried data_ with commitment using CommitmentSchemeVerifier instance.
  EXPECT_TRUE(this->VerifyProof(proof, elements_to_verify));
}

TYPED_TEST(CommitmentScheme, CorruptedProof) {
  // Generate decommitment, using CommitmentSchemeProver instance.
  const auto proof = this->GenerateProof();

  // Fetch data_ in queried locations.
  const std::map<uint64_t, std::vector<std::byte>> elements_to_verify = this->GetElementsToVerify();

  // Construct corrupted proof.
  std::vector<std::byte> corrupted_proof = proof;
  corrupted_proof[this->prng_.template UniformInt<size_t>(0, proof.size() - 1)] ^= std::byte(1);

  // Verify integrity of queried data_ with commitment using CommitmentSchemeVerifier instance.
  EXPECT_FALSE(this->VerifyProof(corrupted_proof, elements_to_verify));
}

TYPED_TEST(CommitmentScheme, CorruptedData) {
  // Generate decommitment, using CommitmentSchemeProver instance.
  const auto proof = this->GenerateProof();

  // Fetch data_ in queried locations.
  const std::map<uint64_t, std::vector<std::byte>> elements_to_verify = this->GetElementsToVerify();

  // Construct corrupted data_.
  std::map<uint64_t, std::vector<std::byte>> corrupted_data = elements_to_verify;
  auto iter = corrupted_data.begin();
  std::advance(iter, this->prng_.template UniformInt<size_t>(0, corrupted_data.size() - 1));
  iter->second[this->prng_.template UniformInt<size_t>(0, this->size_of_element_ - 1)] ^=
      std::byte(1);

  // Verify integrity of queried data_ with commitment using CommitmentSchemeVerifier instance.
  EXPECT_FALSE(this->VerifyProof(proof, corrupted_data));
}

TYPED_TEST(CommitmentScheme, ShortDataOneSegment) {
  // Make shortest possible input.
  this->n_segments_ = 1;
  this->size_of_element_ = TypeParam::kMinElementSize;
  this->n_elements_ = 1;
  this->data_ = this->prng_.RandomByteVector(this->size_of_element_ * this->GetNumElements());
  this->queries_ = this->DrawRandomQueries();

  // Verify completeness.

  // Generate decommitment, using CommitmentSchemeProver instance.
  const auto proof = this->GenerateProof();

  // Fetch data_ in queried locations.
  const std::map<uint64_t, std::vector<std::byte>> elements_to_verify = this->GetElementsToVerify();

  // Verify integrity of queried data_ with commitment using CommitmentSchemeVerifier instance.
  EXPECT_TRUE(this->VerifyProof(proof, elements_to_verify));
}

TYPED_TEST(CommitmentScheme, OutOfRangeQuery) {
  // Set queries to be a single query, out of range ( >= n_elements_).
  const size_t query = this->n_elements_ + this->prng_.template UniformInt<size_t>(0, 10);
  this->queries_ = {query};

  // Try to generate proof.
  EXPECT_ASSERT(this->GenerateProof(), HasSubstr("out of range"));

  // Fetch data_ in queried locations.
  const std::map<uint64_t, std::vector<std::byte>> elements_to_verify = {
      std::make_pair(query, std::vector<std::byte>(this->size_of_element_))};

  // Verify consistency.
  // Proof string must be long enough to contain the commitment.
  const std::vector<std::byte> dummy_proof(100);
  EXPECT_ASSERT(this->VerifyProof(dummy_proof, elements_to_verify), HasSubstr("out of range"));
}

TYPED_TEST(CommitmentScheme, ShortElement) {
  // Generate decommitment, using CommitmentSchemeProver instance.
  const auto proof = this->GenerateProof();

  // Fetch data_ in queried locations.
  const std::map<uint64_t, std::vector<std::byte>> elements_to_verify = this->GetElementsToVerify();

  // Construct corrupted data_ (one element shorter than expected).
  std::map<uint64_t, std::vector<std::byte>> corrupted_data = elements_to_verify;
  auto iter = corrupted_data.begin();
  std::advance(iter, this->prng_.template UniformInt<size_t>(0, corrupted_data.size() - 1));
  iter->second.resize(this->prng_.template UniformInt<size_t>(0, this->size_of_element_ - 1));

  // Verify integrity of queried data_ with commitment using CommitmentSchemeVerifier instance.
  EXPECT_ASSERT(this->VerifyProof(proof, corrupted_data), HasSubstr("Element size mismatch"));
}

TYPED_TEST(CommitmentScheme, LongElement) {
  // Generate decommitment, using CommitmentSchemeProver instance.
  const auto proof = this->GenerateProof();

  // Fetch data_ in queried locations.
  const std::map<uint64_t, std::vector<std::byte>> elements_to_verify = this->GetElementsToVerify();

  // Construct corrupted data_ (with one element longer than expected).
  std::map<uint64_t, std::vector<std::byte>> corrupted_data = elements_to_verify;
  auto iter = corrupted_data.begin();
  std::advance(iter, this->prng_.template UniformInt<size_t>(0, corrupted_data.size() - 1));
  iter->second.resize(this->prng_.template UniformInt<size_t>(
      this->size_of_element_ + 1, this->size_of_element_ + 10));

  // Verify integrity of queried data_ with commitment using CommitmentSchemeVerifier instance.
  EXPECT_ASSERT(this->VerifyProof(proof, corrupted_data), HasSubstr("Element size mismatch"));
}

}  // namespace
}  // namespace starkware
