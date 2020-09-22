#include "starkware/channel/proof_of_work.h"

#include <string>

#include "gtest/gtest.h"

#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

TEST(ProofOfWork, Completeness) {
  Prng prng;
  ProofOfWorkProver pow_prover;

  const size_t work_bits = 15;
  auto witness = pow_prover.Prove(prng.GetPrngState(), work_bits);

  ProofOfWorkVerifier pow_verifier;
  EXPECT_TRUE(pow_verifier.Verify(prng.GetPrngState(), work_bits, witness));
}

TEST(ProofOfWork, Soundness) {
  Prng prng;
  ProofOfWorkProver pow_prover;

  const size_t work_bits = 15;
  auto witness = pow_prover.Prove(prng.GetPrngState(), work_bits);

  ProofOfWorkVerifier pow_verifier;
  EXPECT_FALSE(pow_verifier.Verify(prng.GetPrngState(), work_bits + 1, witness));
  EXPECT_FALSE(pow_verifier.Verify(prng.GetPrngState(), work_bits - 1, witness));
}

TEST(ProofOfWork, BitChange) {
  Prng prng;
  ProofOfWorkProver pow_prover;

  const size_t work_bits = 15;
  auto witness = pow_prover.Prove(prng.GetPrngState(), work_bits);

  ProofOfWorkVerifier pow_verifier;

  for (std::byte& witness_byte : witness) {
    for (size_t bit_index = 0; bit_index < 8; ++bit_index) {
      witness_byte ^= std::byte(1 << bit_index);
      ASSERT_FALSE(pow_verifier.Verify(prng.GetPrngState(), work_bits, witness));
      witness_byte ^= std::byte(1 << bit_index);
    }
  }
}

TEST(ProofOfWork, ParallelCompleteness) {
  Prng prng;
  ProofOfWorkProver pow_prover;
  auto task_manager = TaskManager::CreateInstanceForTesting(8);
  const size_t work_bits = 18;
  auto witness = pow_prover.Prove(prng.GetPrngState(), work_bits, &task_manager, 15);

  ProofOfWorkVerifier pow_verifier;
  EXPECT_TRUE(pow_verifier.Verify(prng.GetPrngState(), work_bits, witness));
}

TEST(ProofOfWork, BadInput) {
  Prng prng;
  ProofOfWorkProver pow_prover;
  ProofOfWorkVerifier pow_verifier;

  const size_t low_work_bits = 0;
  auto dummy_witness = std::vector<std::byte>(1);
  const std::string err_str_low_bits = "At least one bit of work is required.";
  EXPECT_ASSERT(
      pow_prover.Prove(prng.GetPrngState(), low_work_bits), testing::HasSubstr(err_str_low_bits));
  EXPECT_ASSERT(
      pow_verifier.Verify(prng.GetPrngState(), low_work_bits, dummy_witness),
      testing::HasSubstr(err_str_low_bits));

  const size_t high_work_bits = 41;
  const std::string too_many_bit_err_str = "Too many bits of work requested.";
  EXPECT_ASSERT(
      pow_prover.Prove(prng.GetPrngState(), high_work_bits),
      testing::HasSubstr(too_many_bit_err_str));
  EXPECT_ASSERT(
      pow_verifier.Verify(prng.GetPrngState(), high_work_bits, dummy_witness),
      testing::HasSubstr(too_many_bit_err_str));
}

}  // namespace
}  // namespace starkware
