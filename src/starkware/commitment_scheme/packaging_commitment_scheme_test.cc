#include "starkware/commitment_scheme/packaging_commitment_scheme.h"

#include <memory>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/channel/prover_channel_mock.h"
#include "starkware/channel/verifier_channel_mock.h"

#include "starkware/commitment_scheme/commitment_scheme_mock.h"
#include "starkware/crypt_tools/blake2s_160.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::HasSubstr;
using testing::StrictMock;
using testing::Test;

size_t GetNumOfPackages(
    const size_t size_of_element, const uint64_t n_elements_in_segment, const size_t n_segments) {
  // Prover.
  StrictMock<ProverChannelMock> prover_channel;
  size_t prover_factory_input;
  const PackagingCommitmentSchemeProver packaging_prover(
      size_of_element, n_elements_in_segment, n_segments, &prover_channel,
      [&prover_factory_input](
          size_t n_elements_inner_layer) -> std::unique_ptr<CommitmentSchemeProver> {
        prover_factory_input = n_elements_inner_layer;
        return std::make_unique<StrictMock<CommitmentSchemeProverMock>>();
      });
  // Check that packaging_prover calls factory with correct parameters.
  EXPECT_EQ(prover_factory_input, packaging_prover.GetNumOfPackages());

  // Verifier.
  StrictMock<VerifierChannelMock> verifier_channel;
  size_t verifier_factory_input;
  const PackagingCommitmentSchemeVerifier packaging_verifier(
      size_of_element, n_elements_in_segment * n_segments, &verifier_channel,
      [&verifier_factory_input](
          size_t n_elements_inner_layer) -> std::unique_ptr<CommitmentSchemeVerifier> {
        verifier_factory_input = n_elements_inner_layer;
        return std::make_unique<StrictMock<CommitmentSchemeVerifierMock>>();
      });
  // Check that packaging_verifier calls factory with correct parameters.
  EXPECT_EQ(verifier_factory_input, packaging_verifier.GetNumOfPackages());

  EXPECT_EQ(packaging_prover.GetNumOfPackages(), packaging_verifier.GetNumOfPackages());
  return packaging_prover.GetNumOfPackages();
}

void TestGetNumOfPackages(
    const size_t size_of_element, const uint64_t n_elements_in_segment, const size_t n_segments) {
  const size_t n_packages = GetNumOfPackages(size_of_element, n_elements_in_segment, n_segments);
  const size_t n_elements = n_elements_in_segment * n_segments;
  const size_t n_elements_in_package = n_elements / n_packages;
  EXPECT_TRUE(IsPowerOfTwo(n_packages));
  EXPECT_TRUE(IsPowerOfTwo(n_elements_in_package));
  // Checks that n_packages is not too big (equivalently - n_elements_in_package is big enough).
  EXPECT_TRUE(n_elements_in_package * size_of_element >= 2 * Blake2s160::kDigestNumBytes);
  // Checks that n_packages is not too small (equivalently - n_elements_in_package is not too big).
  EXPECT_TRUE(((n_elements_in_package / 2) * size_of_element) < 2 * Blake2s160::kDigestNumBytes);
}

TEST(PackagingCommitmentSchemeProver, GetNumOfPackages) {
  TestGetNumOfPackages(7, 16, 16);
  TestGetNumOfPackages(9, 16, 16);
  TestGetNumOfPackages(32, 16, 8);
  TestGetNumOfPackages(1, 128, 32);
  TestGetNumOfPackages(2 * Blake2s160::kDigestNumBytes, 1, 8);
  // Size of element is zero.
  EXPECT_ASSERT(GetNumOfPackages(0, 32, 20), HasSubstr("least of length"));
  // Num of elements is not power of 2.
  EXPECT_ASSERT(GetNumOfPackages(15, 16, 10), HasSubstr("power of 2"));
  // Size of element is greater than size of package.
  TestGetNumOfPackages(100, 1, 64);
  TestGetNumOfPackages(4 * Blake2s160::kDigestNumBytes, 1, 16);
}

// --------- Prover tests ----------

TEST(PackagingCommitmentSchemeProver, NumSegments) {
  Prng prng;
  const size_t n_segments = Pow2(prng.UniformInt<size_t>(1, 10));
  StrictMock<ProverChannelMock> prover_channel;
  const PackagingCommitmentSchemeProver packaging_prover(
      prng.UniformInt<size_t>(1, sizeof(Blake2s160) * 5), Pow2(prng.UniformInt<size_t>(1, 10)),
      n_segments, &prover_channel,
      [](size_t /*n_elements_inner_layer*/) -> std::unique_ptr<CommitmentSchemeProver> {
        return std::make_unique<StrictMock<CommitmentSchemeProverMock>>();
      });
  EXPECT_EQ(packaging_prover.NumSegments(), n_segments);
}

/*
  Given parameters for PackagingCommitmentSchemeProver, creates PackagingCommitmentSchemeProver
  instance and tests the functions AddSegmentForCommitment and Commit.
*/
void TestAddSegmentForCommitmentAndCommit(
    const size_t size_of_element, const uint64_t n_elements_in_segment, const size_t n_segments) {
  Prng prng;
  StrictMock<ProverChannelMock> prover_channel;
  const size_t segment_index = prng.UniformInt<size_t>(0, n_segments);
  const size_t size_of_data = size_of_element * n_elements_in_segment;
  const std::vector<std::byte> data = prng.RandomByteVector(size_of_data);
  const PackerHasher packer(size_of_element, n_segments * n_elements_in_segment);
  std::vector<std::byte> packed = packer.PackAndHash(data);
  auto inner_commitment_scheme = std::make_unique<StrictMock<CommitmentSchemeProverMock>>();
  EXPECT_CALL(
      *inner_commitment_scheme,
      AddSegmentForCommitment(gsl::span<const std::byte>(packed), segment_index));
  EXPECT_CALL(*inner_commitment_scheme, Commit());
  PackagingCommitmentSchemeProver packaging_prover(
      size_of_element, n_elements_in_segment, n_segments, &prover_channel,
      [&inner_commitment_scheme](
          size_t /*n_elements_inner_layer*/) -> std::unique_ptr<CommitmentSchemeProver> {
        return std::move(inner_commitment_scheme);
      });
  packaging_prover.AddSegmentForCommitment(data, segment_index);
  packaging_prover.Commit();
}

TEST(PackagingCommitmentSchemeProver, AddSegmentForCommitmentAndCommit) {
  TestAddSegmentForCommitmentAndCommit(2 * Blake2s160::kDigestNumBytes, 8, 16);
  TestAddSegmentForCommitmentAndCommit(1, 128, 16);
  TestAddSegmentForCommitmentAndCommit(2 * Blake2s160::kDigestNumBytes, 8, 16);
  TestAddSegmentForCommitmentAndCommit(1, 128, 16);
  TestAddSegmentForCommitmentAndCommit(11, 32, 4);
  TestAddSegmentForCommitmentAndCommit(33, 2, 1);
  TestAddSegmentForCommitmentAndCommit(Blake2s160::kDigestNumBytes, 2, 64);
  TestAddSegmentForCommitmentAndCommit(2 * Blake2s160::kDigestNumBytes + 15, 1, 32);
  TestAddSegmentForCommitmentAndCommit(4 * Blake2s160::kDigestNumBytes, 1, 8);
}

TEST(PackagingCommitmentSchemeProver, AddSegmentForCommitment_AssertsChecks) {
  const size_t size_of_element = 2 * Blake2s160::kDigestNumBytes;
  const uint64_t n_elements_in_segment = 8;
  const size_t n_segments = 16;
  Prng prng;
  StrictMock<ProverChannelMock> prover_channel;
  PackagingCommitmentSchemeProver packaging_prover(
      size_of_element, n_elements_in_segment, n_segments, &prover_channel,
      [](size_t /*n_elements_inner_layer*/) -> std::unique_ptr<CommitmentSchemeProver> {
        return std::make_unique<StrictMock<CommitmentSchemeProverMock>>();
      });
  const size_t size_of_data = size_of_element * packaging_prover.SegmentLengthInElements();
  const std::vector<std::byte> data_too_long(size_of_data + 1);
  const size_t segment_index = prng.UniformInt<size_t>(0, n_segments);
  EXPECT_ASSERT(
      packaging_prover.AddSegmentForCommitment(data_too_long, segment_index),
      HasSubstr("Segment size is"));
}

TEST(PackagingCommitmentSchemeProver, StartDecommitmentPhaseAndDecommit) {
  Prng prng;
  StrictMock<ProverChannelMock> prover_channel;
  const size_t element_size = 11;
  const std::set<uint64_t> queries{1, 3, 30};
  auto inner_commitment_scheme = std::make_unique<StrictMock<CommitmentSchemeProverMock>>();
  // Inner_commitment_scheme packs 2 elements in a package. packaging_prover calls
  // StartDecommitmentPhase of inner_commitment_scheme with a set of package indices of the
  // packages containing queries. There are 4 elements in each package created by packaging_prover,
  // hence, 1 and 3 are in package number 0, and 30 is in package number 7.
  std::set<uint64_t> inner_layer_queries{0, 7};
  EXPECT_CALL(*inner_commitment_scheme, StartDecommitmentPhase(inner_layer_queries))
      .WillOnce(testing::Return(std::vector<uint64_t>{1, 6}));
  // Inner_commitment_scheme needs 8 elements, which are 2 packages (there are 4 elements in a
  // package of packaging_prover). Hence, the size of data sent to decommit is twice the size of
  // hash in bytes.
  EXPECT_CALL(
      *inner_commitment_scheme,
      Decommit(
          testing::Property(&gsl::span<const std::byte>::size, 2 * Blake2s160::kDigestNumBytes)));
  PackagingCommitmentSchemeProver packaging_prover(
      element_size, 16, 8, &prover_channel,
      [&inner_commitment_scheme](
          size_t /*n_elements_inner_layer*/) -> std::unique_ptr<CommitmentSchemeProver> {
        return std::move(inner_commitment_scheme);
      });

  // StartDecommitmentPhase phase.
  auto res = packaging_prover.StartDecommitmentPhase(queries);

  // Vector expected_res contains the rest of the queries required to decommit. For queries 1
  // and 3, we need 0 and 2 because they complete the package of 1 and 3 in the first layer; and 4,
  // 5, 6, 7 because this is the package that is required for calculating the decommit in the second
  // layer. For query 30 we need 28, 29, 31 because they complete the package of 30 in the first
  // layer; and 24, 25, 26, 27 because this is the package that is required for calculating the
  // decommit in the second layer.
  std::vector<uint64_t> expected_res = {0, 2, 28, 29, 31, 4, 5, 6, 7, 24, 25, 26, 27};
  EXPECT_EQ(res, expected_res);

  // Decommit phase.
  const std::vector<std::byte> data = prng.RandomByteVector(expected_res.size() * element_size);
  // SendBytes is called 5 times - 2 times for missing queries of package number 0 and 3 times for
  // missing queries of package number 7.
  const gsl::span<const std::byte> span_data = gsl::make_span(data);
  for (size_t i = 0; i < 5; i++) {
    EXPECT_CALL(prover_channel, SendBytes(span_data.subspan(i * element_size, element_size)));
  }
  packaging_prover.Decommit(data);
}

// ---------- Verifier tests -----------

TEST(PackagingCommitmentSchemeVerifier, ReadCommitment) {
  Prng prng;
  StrictMock<VerifierChannelMock> verifier_channel;
  PackagingCommitmentSchemeVerifier packaging_verifier(
      prng.UniformInt<size_t>(1, sizeof(Blake2s160) * 5), Pow2(prng.UniformInt<size_t>(1, 10)),
      &verifier_channel,
      [](size_t /*n_elements_inner_layer*/) -> std::unique_ptr<CommitmentSchemeVerifier> {
        auto inner_commitment_scheme_verifier =
            std::make_unique<StrictMock<CommitmentSchemeVerifierMock>>();
        EXPECT_CALL(*inner_commitment_scheme_verifier, ReadCommitment());
        return inner_commitment_scheme_verifier;
      });
  packaging_verifier.ReadCommitment();
}

TEST(PackagingCommitmentSchemeVerifier, VerifyIntegritySmallElement) {
  Prng prng;
  StrictMock<VerifierChannelMock> verifier_channel;
  // Disable annotation to avoid running part of code in verifier_channel.ReceiveData() (note that
  // verifier_channel is a mock, there is no real interaction between prover and verifier).
  verifier_channel.DisableAnnotations();
  const size_t element_size_small = 17;
  const size_t n_elements = Pow2(prng.UniformInt<size_t>(4, 10));
  PackagingCommitmentSchemeVerifier packaging_verifier(
      element_size_small, n_elements, &verifier_channel,
      [](size_t /*n_elements_inner_layer*/) -> std::unique_ptr<CommitmentSchemeVerifier> {
        auto inner_commitment_scheme_verifier =
            std::make_unique<StrictMock<CommitmentSchemeVerifierMock>>();
        EXPECT_CALL(*inner_commitment_scheme_verifier, VerifyIntegrity(testing::_));
        return inner_commitment_scheme_verifier;
      });
  // Create 3 elements to verify.
  std::map<uint64_t, std::vector<std::byte>> elements_to_verify;
  elements_to_verify[1] = prng.RandomByteVector(element_size_small);
  elements_to_verify[10] = prng.RandomByteVector(element_size_small);
  elements_to_verify[11] = prng.RandomByteVector(element_size_small);
  // Get required elements from channel. The verifier gets 5 elements from channel: 3 to calculate
  // element number 1, and 2 to calculate elements number 10 and 11.
  // Explanation: number of elements in package is 4 (package size is 2 *
  // Blake2s160::kDigestNumBytes and element size is 17, for full explanation of number of elements
  // in package calculation see Packer_hasher.ComputeNumElementsInPackage). Hence, In order to
  // verify element number 1 the verifier needs elements 0,2,3; and in order to verify elements
  // number 10 and 11 the verifier needs elements number 8, 9.
  EXPECT_CALL(verifier_channel, ReceiveBytes(element_size_small))
      .WillOnce(testing::Return(std::vector<std::byte>(element_size_small)))
      .WillOnce(testing::Return(std::vector<std::byte>(element_size_small)))
      .WillOnce(testing::Return(std::vector<std::byte>(element_size_small)))
      .WillOnce(testing::Return(std::vector<std::byte>(element_size_small)))
      .WillOnce(testing::Return(std::vector<std::byte>(element_size_small)));
  packaging_verifier.VerifyIntegrity(elements_to_verify);
}

TEST(PackagingCommitmentSchemeVerifier, VerifyIntegrityBigElement) {
  Prng prng;
  StrictMock<VerifierChannelMock> verifier_channel;
  // Disables annotation to avoid running part of code in verifier_channel.ReceiveData() (note that
  // verifier_channel is a mock, there is no real interaction between prover and verifier).
  verifier_channel.DisableAnnotations();
  // Size of element is greater than package size (which is 2 * Blake2s160::kDigestNumBytes), hence
  // each package contains a single element. No extra elements are needed to calculate hash of a
  // single element to verify.
  const size_t element_size_big =
      prng.UniformInt<size_t>(sizeof(Blake2s160) * 2, sizeof(Blake2s160) * 5);
  const size_t n_elements = Pow2(prng.UniformInt<size_t>(4, 10));

  // Creates 3 elements to verify.
  std::map<uint64_t, std::vector<std::byte>> elements_to_verify;
  elements_to_verify[1] = prng.RandomByteVector(element_size_big);
  elements_to_verify[10] = prng.RandomByteVector(element_size_big);
  elements_to_verify[11] = prng.RandomByteVector(element_size_big);

  const PackerHasher packer(element_size_big, n_elements);
  auto inner_commitment_scheme_verifier =
      std::make_unique<StrictMock<CommitmentSchemeVerifierMock>>();
  EXPECT_CALL(
      *inner_commitment_scheme_verifier, VerifyIntegrity(packer.PackAndHash(elements_to_verify)));
  PackagingCommitmentSchemeVerifier packaging_verifier(
      element_size_big, n_elements, &verifier_channel,
      [&inner_commitment_scheme_verifier](
          size_t /*n_elements_inner_layer*/) -> std::unique_ptr<CommitmentSchemeVerifier> {
        return std::move(inner_commitment_scheme_verifier);
      });
  packaging_verifier.VerifyIntegrity(elements_to_verify);
}

}  // namespace
}  // namespace starkware
