#include "starkware/fri/fri_committed_layer.h"

#include <set>

#include "starkware/fri/fri_details.h"

namespace starkware {

//  Class FriCommittedLayerByCallback.

void FriCommittedLayerByCallback::Decommit(const std::vector<uint64_t>& queries) {
  (*layer_queries_callback_)(
      fri::details::SecondLayerQueriesToFirstLayerQueries(queries, fri_step_));
}

//  Class FriCommittedLayerByTableProver.

FriCommittedLayerByTableProver::FriCommittedLayerByTableProver(
    size_t fri_step, MaybeOwnedPtr<const FriLayer> layer,
    const TableProverFactory<ExtensionFieldElement>& table_prover_factory,
    const FriParameters& params, size_t layer_num)
    : FriCommittedLayer(fri_step),
      fri_layer_(std::move(layer)),
      params_(params),
      layer_num_(layer_num) {
  uint64_t layer_size = fri_layer_->LayerSize();
  table_prover_ = table_prover_factory(1, layer_size / Pow2(fri_step_), Pow2(fri_step_));
  ASSERT_RELEASE(table_prover_, "No table prover for current layer (probably last layer).");
  Commit();
}

FriCommittedLayerByTableProver::ElementsData FriCommittedLayerByTableProver::EvalAtPoints(
    gsl::span<const uint64_t> required_row_indices) {
  std::vector<gsl::span<const ExtensionFieldElement>> elements_data_spans;
  std::vector<std::vector<ExtensionFieldElement>> elements_data_vectors;

  size_t coset_size = Pow2(params_.fri_step_list[layer_num_]);
  elements_data_vectors.reserve(coset_size);
  elements_data_spans.reserve(coset_size);

  for (uint64_t col = 0; col < coset_size; col++) {
    std::vector<uint64_t> required_indices;
    for (uint64_t row : required_row_indices) {
      required_indices.push_back(row * coset_size + col);
    }

    elements_data_vectors.push_back(fri_layer_->EvalAtPoints(required_indices));
    elements_data_spans.emplace_back(elements_data_vectors.back());
  }
  return ElementsData{
      /*elements=*/std::move(elements_data_spans),
      /*raw_data=*/std::move(elements_data_vectors),
  };
}

void FriCommittedLayerByTableProver::Commit() {
  table_prover_->AddSegmentForCommitment({fri_layer_->GetLayer()}, 0, Pow2(fri_step_));
  table_prover_->Commit();
}

void FriCommittedLayerByTableProver::Decommit(const std::vector<uint64_t>& queries) {
  std::set<RowCol> layer_data_queries, layer_integrity_queries;
  fri::details::NextLayerDataAndIntegrityQueries(
      queries, params_, layer_num_, &layer_data_queries, &layer_integrity_queries);
  std::vector<uint64_t> required_row_indices =
      table_prover_->StartDecommitmentPhase(layer_data_queries, layer_integrity_queries);

  ElementsData elements_data(EvalAtPoints(required_row_indices));

  table_prover_->Decommit(elements_data.elements);
}

}  // namespace starkware
