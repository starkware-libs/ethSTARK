#include "starkware/commitment_scheme/salted_commitment_scheme.h"

#include <memory>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/channel/prover_channel_mock.h"
#include "starkware/channel/verifier_channel_mock.h"

#include "starkware/commitment_scheme/commitment_scheme_mock.h"
#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::HasSubstr;
using testing::StrictMock;

// --------- Prover tests ----------

TEST(SaltedCommitmentSchemeProver, DifferentSalts) {
  Prng prng1(MakeByteArray<0x01>());
  Prng prng2(MakeByteArray<0x02>());
  StrictMock<ProverChannelMock> prover_channel;
  const SaltedCommitmentSchemeProver salted_prover1(
      1, 64, 8, &prover_channel, std::make_unique<StrictMock<CommitmentSchemeProverMock>>(),
      &prng1);

  std::vector<std::array<std::byte, SaltedCommitmentSchemeProver::kSaltNumBytes>> salts1;
  for (uint64_t i = 0; i < 64; ++i) {
    salts1.push_back(salted_prover1.GetSalt(i));
  }
  for (uint64_t i = 1; i < 64; ++i) {
    for (uint64_t j = 0; j < i; ++j) {
      EXPECT_NE(salts1[j], salts1[i]);
    }
  }

  const SaltedCommitmentSchemeProver salted_prover2(
      1, 64, 8, &prover_channel, std::make_unique<StrictMock<CommitmentSchemeProverMock>>(),
      &prng2);
  std::vector<std::array<std::byte, SaltedCommitmentSchemeProver::kSaltNumBytes>> salts2;
  for (uint64_t i = 0; i < 64; ++i) {
    salts2.push_back(salted_prover2.GetSalt(i));
  }
  for (uint64_t i = 0; i < 64; ++i) {
    for (uint64_t j = 0; j < 64; ++j) {
      EXPECT_NE(salts1[i], salts2[j]);
    }
  }
}

TEST(SaltedCommitmentSchemeProver, DeterministicSalts) {
  Prng prng;
  StrictMock<ProverChannelMock> prover_channel;
  const SaltedCommitmentSchemeProver salted_prover1(
      1, 64, 8, &prover_channel, std::make_unique<StrictMock<CommitmentSchemeProverMock>>(), &prng);

  std::vector<std::array<std::byte, SaltedCommitmentSchemeProver::kSaltNumBytes>> salts;
  for (uint64_t i = 0; i < 64; ++i) {
    salts.push_back(salted_prover1.GetSalt(i));
  }
  for (uint64_t i = 0; i < 64; ++i) {
    EXPECT_EQ(salted_prover1.GetSalt(i), salts[i]);
  }
  for (int i = 63; i >= 0; --i) {
    EXPECT_EQ(salted_prover1.GetSalt(i), salts[i]);
  }

  const SaltedCommitmentSchemeProver salted_prover2(
      1, 64, 8, &prover_channel, std::make_unique<StrictMock<CommitmentSchemeProverMock>>(), &prng);
  for (uint64_t i = 0; i < 64; ++i) {
    EXPECT_EQ(salted_prover2.GetSalt(i), salts[i]);
  }
}

TEST(SaltedCommitmentSchemeProver, NumSegments) {
  Prng prng;
  const size_t n_segments = Pow2(prng.UniformInt<size_t>(1, 5));
  StrictMock<ProverChannelMock> prover_channel;
  const SaltedCommitmentSchemeProver salted_prover(
      prng.UniformInt<size_t>(1, sizeof(Blake2s256) * 2),
      n_segments * Pow2(prng.UniformInt<size_t>(1, 5)), n_segments, &prover_channel,
      std::make_unique<StrictMock<CommitmentSchemeProverMock>>(), &prng);
  EXPECT_EQ(salted_prover.NumSegments(), n_segments);
}

TEST(SaltedCommitmentSchemeProver, AddSegmentForCommitment_AssertsChecks) {
  const size_t size_of_element = 2 * Blake2s256::kDigestNumBytes;
  const uint64_t n_elements = 128;
  const size_t n_segments = 16;
  Prng prng;
  StrictMock<ProverChannelMock> prover_channel;
  SaltedCommitmentSchemeProver salted_prover(
      size_of_element, n_elements, n_segments, &prover_channel,
      std::make_unique<StrictMock<CommitmentSchemeProverMock>>(), &prng);
  const size_t size_of_data = size_of_element * salted_prover.SegmentLengthInElements();
  const std::vector<std::byte> data_too_long(size_of_data + 1);
  const size_t segment_index = prng.UniformInt<size_t>(0, n_segments);
  EXPECT_ASSERT(
      salted_prover.AddSegmentForCommitment(data_too_long, segment_index),
      HasSubstr("Segment size is"));
}

// ---------- Verifier tests -----------

TEST(SaltedCommitmentSchemeVerifier, ReadCommitment) {
  Prng prng;
  StrictMock<VerifierChannelMock> verifier_channel;
  auto inner_commitment_scheme_verifier =
      std::make_unique<StrictMock<CommitmentSchemeVerifierMock>>();
  EXPECT_CALL(*inner_commitment_scheme_verifier, ReadCommitment());
  SaltedCommitmentSchemeVerifier salted_verifier(
      prng.UniformInt<size_t>(1, sizeof(Blake2s256) * 2), Pow2(prng.UniformInt<size_t>(1, 10)),
      &verifier_channel, std::move(inner_commitment_scheme_verifier));
  salted_verifier.ReadCommitment();
}

}  // namespace
}  // namespace starkware
