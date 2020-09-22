#ifndef STARKWARE_CHANNEL_PROVER_CHANNEL_H_
#define STARKWARE_CHANNEL_PROVER_CHANNEL_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/channel.h"
#include "starkware/channel/channel_statistics.h"
#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/to_from_string.h"

namespace starkware {

class ProverChannel : public Channel {
 public:
  /*
    Initializes the hash chain to a value based on the public input and the constraints system.
    This ensures that the prover doesn't modify the public input after generating the proof.
  */
  explicit ProverChannel(Prng prng);
  ProverChannel() = default;
  virtual ~ProverChannel() = default;

  void SendData(gsl::span<const std::byte> data, const std::string& annotation = "") {
    SendBytes(data);
    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(annotation + ": Data(" + BytesToHexString(data) + ")", data.size());
    }
    proof_statistics_.data_count += 1;
  }

  template <typename FieldElementT>
  void SendFieldElement(const FieldElementT& value, const std::string& annotation = "") {
    SendFieldElementImpl(value);
    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(
          annotation + ": Field Element(" + value.ToString() + ")", FieldElementT::SizeInBytes());
    }
    proof_statistics_.field_element_count += 1;
  }

  void SendFieldElementSpan(
      gsl::span<const ExtensionFieldElement> values, const std::string& annotation = "") {
    SendFieldElementSpanImpl(values);
    if (AnnotationsEnabled()) {
      std::ostringstream oss;
      oss << annotation << ": Field Elements(" << values << ")";
      AnnotateProverToVerifier(oss.str(), values.size() * ExtensionFieldElement::SizeInBytes());
    }
    proof_statistics_.field_element_count += values.size();
  }

  void SendCommitmentHash(const Blake2s256& hash, const std::string& annotation = "") {
    SendBytes(hash.GetDigest());
    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(
          annotation + ": Hash(" + hash.ToString() + ")", Blake2s256::kDigestNumBytes);
    }
    proof_statistics_.commitment_count += 1;
    proof_statistics_.hash_count += 1;
  }

  ExtensionFieldElement ReceiveFieldElement(const std::string& annotation = "") {
    ExtensionFieldElement field_element = ReceiveFieldElementImpl();
    if (AnnotationsEnabled()) {
      AnnotateVerifierToProver(annotation + ": Field Element(" + field_element.ToString() + ")");
    }
    return field_element;
  }

  ExtensionFieldElement GetRandomFieldElementFromVerifier(const std::string& annotation) override {
    return ReceiveFieldElement(annotation);
  }

  void SendDecommitmentNode(const Blake2s256& hash_node, const std::string& annotation = "") {
    SendBytes(hash_node.GetDigest());
    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(
          annotation + ": Hash(" + hash_node.ToString() + ")", Blake2s256::kDigestNumBytes);
    }
    proof_statistics_.hash_count++;
  }

  /*
    Receives a random number from the verifier. The number should be chosen uniformly in the
    range [0, upper_bound).
  */
  uint64_t ReceiveNumber(uint64_t upper_bound, const std::string& annotation = "") {
    uint64_t number = ReceiveNumberImpl(upper_bound);
    if (AnnotationsEnabled()) {
      AnnotateVerifierToProver(annotation + ": Number(" + std::to_string(number) + ")");
    }
    return number;
  }

  uint64_t GetRandomNumberFromVerifier(
      uint64_t upper_bound, const std::string& annotation) override {
    return ReceiveNumber(upper_bound, annotation);
  }

  /*
    Finds a nonce (8 bytes) for which blake(blake(magic || prng_seed || work_bits) || nonce)
    has security_bits leading zero bits. Then, appends the nonce to the proof.
    This is done using ProofOfWork.
  */
  void ApplyProofOfWork(size_t security_bits) override;
  std::vector<std::byte> GetProof() const;

  // ===============================================================================================
  // The following methods are virtual to allow implementing mock for test purposes.
  // ===============================================================================================

  /*
    SendBytes writes raw bytes to the proof and updates the hash chain.
  */
  virtual void SendBytes(gsl::span<const std::byte> raw_bytes);

  /*
    ReceiveBytes is a method that uses a hash chain and every time it is called the requested
    number of random bytes are returned and the hash chain is updated.
  */
  virtual std::vector<std::byte> ReceiveBytes(size_t num_bytes);

 protected:
  virtual void SendFieldElementImpl(const BaseFieldElement& value);
  virtual void SendFieldElementImpl(const ExtensionFieldElement& value);
  virtual void SendFieldElementSpanImpl(gsl::span<const ExtensionFieldElement> values);
  virtual ExtensionFieldElement ReceiveFieldElementImpl();
  virtual uint64_t ReceiveNumberImpl(uint64_t upper_bound);

 private:
  Prng prng_;
  std::vector<std::byte> proof_{};
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_PROVER_CHANNEL_H_
