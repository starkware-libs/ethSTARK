#include "starkware/commitment_scheme/packaging_commitment_scheme.h"

#include <algorithm>
#include <string>
#include <utility>

#include "starkware/commitment_scheme/packer_hasher.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {

PackagingCommitmentSchemeProver::PackagingCommitmentSchemeProver(
    size_t size_of_element, uint64_t n_elements_in_segment, size_t n_segments,
    ProverChannel* channel,
    const PackagingCommitmentSchemeProverFactory& inner_commitment_scheme_factory)
    : size_of_element_(size_of_element),
      n_elements_in_segment_(n_elements_in_segment),
      n_segments_(n_segments),
      channel_(channel),
      packer_(PackerHasher(size_of_element_, n_segments_ * n_elements_in_segment_)),
      inner_commitment_scheme_(inner_commitment_scheme_factory(packer_.k_n_packages)),
      missing_element_queries_({}) {}

size_t PackagingCommitmentSchemeProver::NumSegments() const { return n_segments_; }

size_t PackagingCommitmentSchemeProver::GetNumOfPackages() const { return packer_.k_n_packages; }

uint64_t PackagingCommitmentSchemeProver::SegmentLengthInElements() const {
  return n_elements_in_segment_;
}

void PackagingCommitmentSchemeProver::AddSegmentForCommitment(
    gsl::span<const std::byte> segment_data, size_t segment_index) {
  ASSERT_RELEASE(
      segment_data.size() == n_elements_in_segment_ * size_of_element_,
      "Segment size is " + std::to_string(segment_data.size()) + " instead of the expected " +
          std::to_string(size_of_element_ * n_elements_in_segment_));
  inner_commitment_scheme_->AddSegmentForCommitment(
      packer_.PackAndHash(segment_data), segment_index);
}

void PackagingCommitmentSchemeProver::Commit() { inner_commitment_scheme_->Commit(); }

std::vector<uint64_t> PackagingCommitmentSchemeProver::StartDecommitmentPhase(
    const std::set<uint64_t>& queries) {
  queries_ = queries;
  // Compute the missing elements required to compute hashes for the current layer.
  missing_element_queries_ = packer_.ElementsRequiredToComputeHashes(queries_);

  // Translate query indices from element indices to package indices, since this is what
  // inner_commitment_scheme handles.
  std::set<uint64_t> package_queries_to_inner_layer;
  for (uint64_t q : queries_) {
    package_queries_to_inner_layer.insert(q / packer_.k_n_elements_in_package);
  }
  // Send required queries to inner_commitment_scheme_ and get required queries needed for it.
  auto missing_package_queries_inner_layer =
      inner_commitment_scheme_->StartDecommitmentPhase(package_queries_to_inner_layer);
  // Translate inner layer queries to current layer requests for element.
  const auto missing_element_queries_to_inner_layer =
      packer_.GetElementsInPackages(missing_package_queries_inner_layer);

  n_missing_elements_for_inner_layer_ = missing_element_queries_to_inner_layer.size();
  std::vector<uint64_t> all_missing_elements;
  all_missing_elements.reserve(
      missing_element_queries_.size() + n_missing_elements_for_inner_layer_);
  // missing_element_queries and missing_element_queries_to_inner_layer are disjoint sets.
  all_missing_elements.insert(
      all_missing_elements.end(), missing_element_queries_.begin(), missing_element_queries_.end());
  all_missing_elements.insert(
      all_missing_elements.end(), missing_element_queries_to_inner_layer.begin(),
      missing_element_queries_to_inner_layer.end());

  return all_missing_elements;
}

void PackagingCommitmentSchemeProver::Decommit(gsl::span<const std::byte> elements_data) {
  ASSERT_RELEASE(
      elements_data.size() == size_of_element_ * (missing_element_queries_.size() +
                                                  n_missing_elements_for_inner_layer_),
      "Data size of data given in Decommit doesn't fit request in StartDecommitmentPhase.");

  // Send to channel the elements current packaging commitment scheme got according to its request
  // in StartDecommitmentPhase.
  for (size_t i = 0; i < missing_element_queries_.size(); i++) {
    const auto bytes_to_send = elements_data.subspan(i * size_of_element_, size_of_element_);
    channel_->SendData(
        bytes_to_send,
        "To complete packages, element #" + std::to_string(missing_element_queries_[i]));
  }

  // Pack and hash the data inner_commitment_scheme_ requested in StartDecommitmentPhase and send
  // it to inner_commitment_scheme_.
  std::vector<std::byte> data_for_inner_layer = packer_.PackAndHash(elements_data.subspan(
      missing_element_queries_.size() * size_of_element_,
      n_missing_elements_for_inner_layer_ * size_of_element_));
  inner_commitment_scheme_->Decommit(data_for_inner_layer);
}

PackagingCommitmentSchemeVerifier::PackagingCommitmentSchemeVerifier(
    size_t size_of_element, uint64_t n_elements, VerifierChannel* channel,
    const PackagingCommitmentSchemeVerifierFactory& inner_commitment_scheme_factory)
    : size_of_element_(size_of_element),
      n_elements_(n_elements),
      channel_(channel),
      packer_(PackerHasher(size_of_element, n_elements_)),
      inner_commitment_scheme_(inner_commitment_scheme_factory(packer_.k_n_packages)) {}

void PackagingCommitmentSchemeVerifier::ReadCommitment() {
  inner_commitment_scheme_->ReadCommitment();
}

bool PackagingCommitmentSchemeVerifier::VerifyIntegrity(
    const std::map<uint64_t, std::vector<std::byte>>& elements_to_verify) {
  // Get missing elements (i.e., ones in the same packages as at least one element in
  // elements_to_verify, but that are not elements that the verifier actually asked about) by
  // reading from decommitment. For example - if elements_to_verify equals to {2,8} and there are 4
  // elements in each package then missing_elements_idxs = {0,1,3,9,10,11}: 0,1,3 to verify the
  // package for element 2 and 9,10,11 to verify the package for element 8.
  const std::vector<uint64_t> missing_elements_idxs =
      packer_.ElementsRequiredToComputeHashes(Keys(elements_to_verify));

  std::map<uint64_t, std::vector<std::byte>> full_data_to_verify(elements_to_verify);
  for (const uint64_t missing_element_idx : missing_elements_idxs) {
    full_data_to_verify[missing_element_idx] = channel_->ReceiveData(
        size_of_element_, "To complete packages, element #" + std::to_string(missing_element_idx));
  }

  // Convert data to bytes.
  const std::map<uint64_t, std::vector<std::byte>> bytes_to_verify =
      packer_.PackAndHash(full_data_to_verify);
  return inner_commitment_scheme_->VerifyIntegrity(bytes_to_verify);
}

size_t PackagingCommitmentSchemeVerifier::GetNumOfPackages() const { return packer_.k_n_packages; }

}  // namespace starkware
