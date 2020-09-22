/*
  Proof of work prover and verifier. The algorithm:
  Find a nonce of size 8 bytes for which hash(hash(magic || seed) || nonce) has work_bits leading
  zero bits (most significant bits).
*/
#ifndef STARKWARE_CHANNEL_PROOF_OF_WORK_H_
#define STARKWARE_CHANNEL_PROOF_OF_WORK_H_

#include <vector>

#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/utils/task_manager.h"

namespace starkware {

class ProofOfWorkProver {
  static_assert(Blake2s256::kDigestNumBytes >= 8, "Digest size must be at least 64 bits.");

 public:
  /*
    Returns nonce for which hash(hash(magic || seed || work_bits) || nonce) has work_bits leading
    zeros.
  */
  std::vector<std::byte> Prove(
      gsl::span<const std::byte> seed, size_t work_bits, uint64_t log_chunk_size = 20);
  std::vector<std::byte> Prove(
      gsl::span<const std::byte> seed, size_t work_bits, TaskManager* task_manager,
      uint64_t log_chunk_size = 20);
};

class ProofOfWorkVerifier {
  static_assert(Blake2s256::kDigestNumBytes >= 8, "Digest size must be at least 64 bits.");

 public:
  static const size_t kNonceBytes = sizeof(uint64_t);

  /*
    Returns true iff hash(hash(magic || seed || work_bits) || nonce) has work_bits leading zeros.
  */
  bool Verify(
      gsl::span<const std::byte> seed, size_t work_bits, gsl::span<const std::byte> nonce_bytes);
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_PROOF_OF_WORK_H_
