#ifndef STARKWARE_CHANNEL_VERIFIER_CHANNEL_H_
#define STARKWARE_CHANNEL_VERIFIER_CHANNEL_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/channel/channel.h"
#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/to_from_string.h"

namespace starkware {

class VerifierChannel : public Channel {
 public:
  /*
    Initializes the hash chain to a value based on the public input and the constraints system. This
    ensures that the prover doesn't modify the public input after generating the proof.
  */
  explicit VerifierChannel(Prng prng, gsl::span<const std::byte> proof);
  VerifierChannel() = default;
  virtual ~VerifierChannel() = default;

  /*
    Generates a random number for the verifier, sends it to the prover and returns it. The number
    should be chosen uniformly in the range [0, upper_bound).
  */
  uint64_t GetAndSendRandomNumber(uint64_t upper_bound, const std::string& annotation = "") {
    uint64_t number = GetAndSendRandomNumberImpl(upper_bound);
    if (AnnotationsEnabled()) {
      AnnotateVerifierToProver(annotation + ": Number(" + std::to_string(number) + ")");
    }
    return number;
  }

  uint64_t GetRandomNumberFromVerifier(
      uint64_t upper_bound, const std::string& annotation) override {
    return GetAndSendRandomNumber(upper_bound, annotation);
  }

  /*
    Generates a random field element from the requested field, sends it to the prover and returns
    it.
  */
  ExtensionFieldElement GetAndSendRandomFieldElement(const std::string& annotation = "") {
    ExtensionFieldElement field_element = GetAndSendRandomFieldElementImpl();
    if (AnnotationsEnabled()) {
      AnnotateVerifierToProver(annotation + ": Field Element(" + field_element.ToString() + ")");
    }
    return field_element;
  }

  ExtensionFieldElement GetRandomFieldElementFromVerifier(const std::string& annotation) override {
    return GetAndSendRandomFieldElement(annotation);
  }

  Blake2s256 ReceiveCommitmentHash(const std::string& annotation = "") {
    Blake2s256 hash = Blake2s256::InitDigestTo(ReceiveBytes(Blake2s256::kDigestNumBytes));
    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(
          annotation + ": Hash(" + hash.ToString() + ")", Blake2s256::kDigestNumBytes);
    }
    proof_statistics_.commitment_count += 1;
    proof_statistics_.hash_count += 1;
    return hash;
  }

  template <typename FieldElementT>
  FieldElementT ReceiveFieldElement(const std::string& annotation = "") {
    FieldElementT field_element = FieldElementT::Uninitialized();

    // NOLINTNEXTLINE: clang-tidy if constexpr bug.
    if constexpr (std::is_same_v<FieldElementT, BaseFieldElement>) {
      field_element = ReceiveBaseFieldElementImpl();
    } else {  // NOLINT: clang-tidy if constexpr bug.
      field_element = ReceiveExtensionFieldElementImpl();
    }

    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(
          annotation + ": Field Element(" + field_element.ToString() + +")",
          FieldElementT::SizeInBytes());
    }
    proof_statistics_.field_element_count += 1;
    return field_element;
  }

  void ReceiveFieldElementSpan(
      gsl::span<ExtensionFieldElement> span, const std::string& annotation = "") {
    ReceiveFieldElementSpanImpl(span);
    if (AnnotationsEnabled()) {
      std::ostringstream oss;
      oss << annotation << ": Field Elements(" << span << ")";
      AnnotateProverToVerifier(oss.str(), span.size() * ExtensionFieldElement::SizeInBytes());
    }
    proof_statistics_.field_element_count += span.size();
  }

  Blake2s256 ReceiveDecommitmentNode(const std::string& annotation = "") {
    Blake2s256 hash = Blake2s256::InitDigestTo(ReceiveBytes(Blake2s256::kDigestNumBytes));
    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(
          annotation + ": Hash(" + hash.ToString() + ")", Blake2s256::kDigestNumBytes);
    }

    proof_statistics_.hash_count++;

    return hash;
  }

  std::vector<std::byte> ReceiveData(size_t num_bytes, const std::string& annotation = "") {
    const std::vector<std::byte> data = ReceiveBytes(num_bytes);
    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(annotation + ": Data(" + BytesToHexString(data) + ")", num_bytes);
    }
    proof_statistics_.data_count += 1;
    return data;
  }

  /*
    Returns true if the proof was fully read.
  */
  bool IsEndOfProof();

  void ApplyProofOfWork(size_t security_bits) override;

  // ===============================================================================================
  // The following methods are virtual to allow implementing mock for test purposes.
  // ===============================================================================================

  /*
    SendBytes just updates the hash chain but has no other effect.
  */
  virtual void SendBytes(gsl::span<const std::byte> raw_bytes);

  /*
    ReceiveBytes reads bytes from the proof and updates the hash chain accordingly.
  */
  virtual std::vector<std::byte> ReceiveBytes(size_t num_bytes);

 protected:
  /*
    Generates a random number, sends it to the prover and returns it to the caller.
    The number should be chosen uniformly in the range [0, upper_bound).
  */
  virtual uint64_t GetAndSendRandomNumberImpl(uint64_t upper_bound);
  virtual ExtensionFieldElement GetAndSendRandomFieldElementImpl();
  virtual BaseFieldElement ReceiveBaseFieldElementImpl();
  virtual ExtensionFieldElement ReceiveExtensionFieldElementImpl();
  virtual void ReceiveFieldElementSpanImpl(gsl::span<ExtensionFieldElement> span);

  // Randomness generator.
  virtual uint64_t GetRandomNumber(uint64_t upper_bound);
  virtual ExtensionFieldElement GetRandomFieldElement();

 private:
  /*
    Send() functions should not be used by the verifier as they cannot be simulated by a
    non-interactive prover. However, internally the GetAndSendRandom*() functions use this API.
    This API is virtual only in order to allow for a mock of verifier_channel.
  */
  virtual void SendNumber(uint64_t number);
  virtual void SendFieldElement(const ExtensionFieldElement& value);
  Prng prng_;
  const std::vector<std::byte> proof_;
  size_t proof_read_index_ = 0;
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_VERIFIER_CHANNEL_H_
