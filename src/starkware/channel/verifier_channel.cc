#include "starkware/channel/verifier_channel.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "starkware/channel/annotation_scope.h"
#include "starkware/channel/channel_utils.h"
#include "starkware/channel/proof_of_work.h"
#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/utils/serialization.h"

namespace starkware {

void VerifierChannel::SendNumber(uint64_t number) {
  std::array<std::byte, sizeof(uint64_t)> bytes{};
  Serialize(number, bytes);
  SendBytes(bytes);
}

void VerifierChannel::SendFieldElement(const ExtensionFieldElement& value) {
  std::vector<std::byte> raw_bytes(ExtensionFieldElement::SizeInBytes());
  value.ToBytes(raw_bytes);
  SendBytes(raw_bytes);
}

BaseFieldElement VerifierChannel::ReceiveBaseFieldElementImpl() {
  return BaseFieldElement::FromBytes(ReceiveBytes(BaseFieldElement::SizeInBytes()));
}

ExtensionFieldElement VerifierChannel::ReceiveExtensionFieldElementImpl() {
  return ExtensionFieldElement::FromBytes(ReceiveBytes(ExtensionFieldElement::SizeInBytes()));
}

void VerifierChannel::ReceiveFieldElementSpanImpl(gsl::span<ExtensionFieldElement> span) {
  const size_t size_in_bytes = ExtensionFieldElement::SizeInBytes();
  const size_t n_elements = span.size();
  std::vector<std::byte> field_element_vector_bytes = ReceiveBytes(size_in_bytes * n_elements);
  auto span_bytes = gsl::make_span(field_element_vector_bytes);
  for (size_t i = 0; i < n_elements; i++) {
    span.at(i) =
        ExtensionFieldElement::FromBytes(span_bytes.subspan(i * size_in_bytes, size_in_bytes));
  }
}

uint64_t VerifierChannel::GetAndSendRandomNumberImpl(uint64_t upper_bound) {
  uint64_t number = GetRandomNumber(upper_bound);
  // NOTE: Must be coupled with GetRandomNumber (for the non-interactive hash chain).
  SendNumber(number);
  return number;
}

ExtensionFieldElement VerifierChannel::GetAndSendRandomFieldElementImpl() {
  ExtensionFieldElement field_element = GetRandomFieldElement();
  SendFieldElement(field_element);
  return field_element;
}

VerifierChannel::VerifierChannel(Prng prng, gsl::span<const std::byte> proof)
    : prng_(std::move(prng)), proof_(proof.begin(), proof.end()) {}

void VerifierChannel::SendBytes(gsl::span<const std::byte> /*raw_bytes*/) {
  // For the non-interactive verifier implementation this function does nothing.
  // Any updates to the hash chain are the responsibility of functions requiring randomness.
  ASSERT_RELEASE(!in_query_phase_, "Verifier can't send randomness after query phase has begun.");
}

std::vector<std::byte> VerifierChannel::ReceiveBytes(const size_t num_bytes) {
  ASSERT_RELEASE(proof_read_index_ + num_bytes <= proof_.size(), "Proof too short.");
  std::vector<std::byte> raw_bytes(
      proof_.begin() + proof_read_index_, proof_.begin() + proof_read_index_ + num_bytes);
  proof_read_index_ += num_bytes;
  if (!in_query_phase_) {
    prng_.MixSeedWithBytes(raw_bytes);
  }
  proof_statistics_.byte_count += raw_bytes.size();
  return raw_bytes;
}

uint64_t VerifierChannel::GetRandomNumber(uint64_t upper_bound) {
  ASSERT_RELEASE(!in_query_phase_, "Verifier can't send randomness after query phase has begun.");
  return ChannelUtils::GetRandomNumber(upper_bound, &prng_);
}

ExtensionFieldElement VerifierChannel::GetRandomFieldElement() {
  ASSERT_RELEASE(!in_query_phase_, "Verifier can't send randomness after query phase has begun.");
  return ExtensionFieldElement::RandomElement(&prng_);
}

void VerifierChannel::ApplyProofOfWork(size_t security_bits) {
  if (security_bits == 0) {
    return;
  }

  AnnotationScope scope(this, "Proof of Work");

  ProofOfWorkVerifier pow_verifier;
  const auto prev_state = prng_.GetPrngState();
  auto witness = ReceiveData(ProofOfWorkVerifier::kNonceBytes, "POW");
  ASSERT_RELEASE(pow_verifier.Verify(prev_state, security_bits, witness), "Wrong proof of work.");
}

bool VerifierChannel::IsEndOfProof() { return proof_read_index_ >= proof_.size(); }

}  // namespace starkware
