#include "starkware/commitment_scheme/salted_commitment_scheme.h"

#include <algorithm>
#include <string>
#include <utility>

#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/serialization.h"

namespace starkware {

SaltedCommitmentSchemeProver::SaltedCommitmentSchemeProver(
    size_t size_of_element, uint64_t n_elements, size_t n_segments, ProverChannel* channel,
    std::unique_ptr<CommitmentSchemeProver> inner_commitment_scheme, Prng* prng)
    : size_of_element_(size_of_element),
      n_elements_(n_elements),
      n_segments_(n_segments),
      channel_(channel),
      inner_commitment_scheme_(std::move(inner_commitment_scheme)),
      prng_(prng) {
  ASSERT_RELEASE(prng != nullptr, "Null pointer was given.");
}

size_t SaltedCommitmentSchemeProver::NumSegments() const { return n_segments_; }

uint64_t SaltedCommitmentSchemeProver::SegmentLengthInElements() const {
  return SafeDiv(n_elements_, n_segments_);
}

void SaltedCommitmentSchemeProver::AddSegmentForCommitment(
    gsl::span<const std::byte> segment_data, size_t segment_index) {
  ASSERT_RELEASE(
      segment_data.size() == SegmentLengthInElements() * size_of_element_,
      "Segment size is " + std::to_string(segment_data.size()) + " instead of the expected " +
          std::to_string(size_of_element_ * SegmentLengthInElements()) + ".");
  inner_commitment_scheme_->AddSegmentForCommitment(
      HashSegment(segment_data, segment_index), segment_index);
}

void SaltedCommitmentSchemeProver::Commit() { inner_commitment_scheme_->Commit(); }

std::vector<uint64_t> SaltedCommitmentSchemeProver::StartDecommitmentPhase(
    const std::set<uint64_t>& queries) {
  queries_ = queries;
  return inner_commitment_scheme_->StartDecommitmentPhase(queries);
}

void SaltedCommitmentSchemeProver::Decommit(gsl::span<const std::byte> elements_data) {
  ASSERT_RELEASE(elements_data.empty(), "element_data is expected to be empty.");
  for (uint64_t query : queries_) {
    const auto salt = GetSalt(query);
    channel_->SendData(salt, "salt " + std::to_string(query));
  }
  inner_commitment_scheme_->Decommit(elements_data);
}

auto SaltedCommitmentSchemeProver::GetSalt(uint64_t index) const
    -> std::array<std::byte, kSaltNumBytes> {
  std::array<std::byte, sizeof(uint64_t)> bytes{};
  Serialize(index, bytes);
  Prng prng = prng_->Clone();
  prng.MixSeedWithBytes(bytes);
  std::array<std::byte, kSaltNumBytes> salt{};
  prng.GetRandomBytes(salt);
  return salt;
}

void HashElement(
    gsl::span<const std::byte> element_span, gsl::span<const std::byte> salt,
    gsl::span<std::byte> output_span) {
  std::vector<std::byte> salted_data(element_span.begin(), element_span.end());
  std::copy(salt.begin(), salt.end(), std::back_inserter(salted_data));
  const auto& hash = Blake2s256::HashBytesWithLength(salted_data).GetDigest();
  std::copy(hash.begin(), hash.end(), output_span.begin());
}

std::vector<std::byte> SaltedCommitmentSchemeProver::HashSegment(
    gsl::span<const std::byte> segment_data, size_t segment_index) {
  std::vector<std::byte> hashed_data(SegmentLengthInElements() * kSizeOfElement);
  auto hashed_data_span = gsl::make_span(hashed_data);

  for (size_t i = 0; i < SegmentLengthInElements(); ++i) {
    HashElement(
        segment_data.subspan(size_of_element_ * i, size_of_element_),
        GetSalt(i + segment_index * SegmentLengthInElements()),
        hashed_data_span.subspan(kSizeOfElement * i, kSizeOfElement));
  }

  return hashed_data;
}

SaltedCommitmentSchemeVerifier::SaltedCommitmentSchemeVerifier(
    size_t size_of_element, uint64_t n_elements, VerifierChannel* channel,
    std::unique_ptr<CommitmentSchemeVerifier> inner_commitment_scheme)
    : size_of_element_(size_of_element),
      n_elements_(n_elements),
      channel_(channel),
      inner_commitment_scheme_(std::move(inner_commitment_scheme)) {}

void SaltedCommitmentSchemeVerifier::ReadCommitment() {
  inner_commitment_scheme_->ReadCommitment();
}

bool SaltedCommitmentSchemeVerifier::VerifyIntegrity(
    const std::map<uint64_t, std::vector<std::byte>>& elements_to_verify) {
  // Receive salts and compute decommitment data of the inner commitment scheme.
  std::map<uint64_t, std::vector<std::byte>> bytes_to_verify;
  for (auto const& [query, value] : elements_to_verify) {
    ASSERT_RELEASE(query < n_elements_, "Query out of range.");
    ASSERT_RELEASE(value.size() == size_of_element_, "Element size mismatches.");
    bytes_to_verify[query] = std::vector<std::byte>(SaltedCommitmentSchemeProver::kSizeOfElement);
    HashElement(
        value,
        channel_->ReceiveData(
            SaltedCommitmentSchemeProver::kSaltNumBytes, "salt " + std::to_string(query)),
        bytes_to_verify[query]);
  }
  return inner_commitment_scheme_->VerifyIntegrity(bytes_to_verify);
}

}  // namespace starkware
