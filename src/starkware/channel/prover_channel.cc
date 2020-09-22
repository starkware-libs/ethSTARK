#include "starkware/channel/prover_channel.h"

#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "glog/logging.h"
#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/channel/channel_utils.h"
#include "starkware/channel/proof_of_work.h"
#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/randomness/prng.h"
#include "starkware/utils/serialization.h"

namespace starkware {

void ProverChannel::SendFieldElementImpl(const BaseFieldElement& value) {
  std::vector<std::byte> raw_bytes(BaseFieldElement::SizeInBytes());
  value.ToBytes(raw_bytes);
  SendBytes(raw_bytes);
}

void ProverChannel::SendFieldElementImpl(const ExtensionFieldElement& value) {
  std::vector<std::byte> raw_bytes(ExtensionFieldElement::SizeInBytes());
  value.ToBytes(raw_bytes);
  SendBytes(raw_bytes);
}

void ProverChannel::SendFieldElementSpanImpl(gsl::span<const ExtensionFieldElement> values) {
  const size_t size_in_bytes = ExtensionFieldElement::SizeInBytes();
  std::vector<std::byte> raw_bytes(values.size() * size_in_bytes);
  auto raw_bytes_span = gsl::make_span(raw_bytes);
  for (size_t i = 0; i < values.size(); i++) {
    values[i].ToBytes(raw_bytes_span.subspan(i * size_in_bytes, size_in_bytes));
  }
  SendBytes(raw_bytes);
}

ProverChannel::ProverChannel(Prng prng) : prng_(std::move(prng)) {}

void ProverChannel::SendBytes(gsl::span<const std::byte> raw_bytes) {
  for (std::byte raw_byte : raw_bytes) {
    proof_.push_back(raw_byte);
  }
  if (!in_query_phase_) {
    prng_.MixSeedWithBytes(raw_bytes);
  }
  proof_statistics_.byte_count += raw_bytes.size();
}

std::vector<std::byte> ProverChannel::ReceiveBytes(const size_t num_bytes) {
  ASSERT_RELEASE(!in_query_phase_, "Prover can't receive randomness after query phase has begun.");
  std::vector<std::byte> bytes(num_bytes);
  prng_.GetRandomBytes(bytes);
  return bytes;
}

ExtensionFieldElement ProverChannel::ReceiveFieldElementImpl() {
  ASSERT_RELEASE(!in_query_phase_, "Prover can't receive randomness after query phase has begun.");
  VLOG(4) << "Prng state: " << BytesToHexString(prng_.GetPrngState());
  return ExtensionFieldElement::RandomElement(&prng_);
}

/*
  Receives a random number from the verifier. The number should be chosen uniformly in the range
  [0, upper_bound).
*/
uint64_t ProverChannel::ReceiveNumberImpl(uint64_t upper_bound) {
  ASSERT_RELEASE(!in_query_phase_, "Prover can't receive randomness after query phase has begun.");
  return ChannelUtils::GetRandomNumber(upper_bound, &prng_);
}

void ProverChannel::ApplyProofOfWork(size_t security_bits) {
  if (security_bits == 0) {
    return;
  }

  AnnotationScope scope(this, "Proof of Work");

  ProofOfWorkProver pow_prover;
  SendData(pow_prover.Prove(prng_.GetPrngState(), security_bits), "POW");
}

std::vector<std::byte> ProverChannel::GetProof() const { return proof_; }

}  // namespace starkware
