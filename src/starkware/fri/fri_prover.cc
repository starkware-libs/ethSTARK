#include "starkware/fri/fri_prover.h"

#include "glog/logging.h"

#include "starkware/algebra/lde/lde_manager.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/profiling.h"

namespace starkware {

using starkware::fri::details::ChooseQueryIndices;

FriProver::FriProver(
    MaybeOwnedPtr<ProverChannel> channel,
    MaybeOwnedPtr<TableProverFactory<ExtensionFieldElement>> table_prover_factory,
    MaybeOwnedPtr<FriParameters> params, std::vector<ExtensionFieldElement>&& witness,
    MaybeOwnedPtr<FirstLayerCallback> first_layer_queries_callback)
    : channel_(std::move(channel)),
      table_prover_factory_(std::move(table_prover_factory)),
      params_(std::move(params)),
      witness_(std::move(witness)),
      n_layers_(params_->fri_step_list.size()) {
  committed_layers_.reserve(n_layers_);
  committed_layers_.emplace_back(UseMovedValue(FriCommittedLayerByCallback(
      params_->fri_step_list.at(0), std::move(first_layer_queries_callback))));
}

MaybeOwnedPtr<const FriLayer> FriProver::CommitmentPhase() {
  ProfilingBlock profiling_block("FRI commit phase");
  size_t basis_index = 0;

  ASSERT_RELEASE(
      witness_.size() == params_->domain.Size(),
      "Witness should be an evaluation on the entire domain.");

  // Initialize first layer (layer 0).
  MaybeOwnedPtr<const FriLayer> first_layer(
      UseMovedValue(FriLayerReal(std::move(witness_), params_->domain)));

  // Create other layers.
  MaybeOwnedPtr<const FriLayer> current_layer = UseOwned(first_layer);
  for (size_t layer_num = 1; layer_num <= n_layers_; ++layer_num) {
    const size_t fri_step = params_->fri_step_list[layer_num - 1];
    const size_t next_fri_step = (layer_num < n_layers_) ? params_->fri_step_list[layer_num] : 0;

    ASSERT_RELEASE(layer_num == 1 || fri_step != 0, "Only first layer may have fri_step = 0.");

    AnnotationScope scope(channel_.get(), "Layer " + std::to_string(layer_num));

    current_layer = CreateNextFriLayer(std::move(current_layer), fri_step, &basis_index);
    current_layer = UseMovedValue(FriLayerReal(std::move(current_layer)));
    MaybeOwnedPtr<const FriLayer> new_layer = UseOwned(current_layer);

    // For the last layer, skip creating a committed layer.
    if (layer_num == n_layers_) {
      break;
    }

    // Create a commited layer.
    committed_layers_.emplace_back(UseMovedValue(FriCommittedLayerByTableProver(
        next_fri_step, std::move(current_layer), *table_prover_factory_, *params_, layer_num)));
    current_layer = UseOwned(new_layer);
  }

  return current_layer;
}

MaybeOwnedPtr<const FriLayer> FriProver::CreateNextFriLayer(
    MaybeOwnedPtr<const FriLayer> current_layer, size_t fri_step, size_t* basis_index) {
  std::optional<ExtensionFieldElement> eval_point = std::nullopt;
  // If layer reduction > 1, iteratively compute next layer.
  if (fri_step != 0) {
    eval_point = channel_->ReceiveFieldElement("Evaluation point");

    for (size_t j = 0; j < fri_step; ++j, ++(*basis_index)) {
      current_layer = UseMovedValue(FriLayerProxy(std::move(current_layer), *eval_point));
      eval_point = (*eval_point) * (*eval_point);
    }
  }

  return current_layer;
}

void FriProver::SendLastLayer(MaybeOwnedPtr<const FriLayer>&& last_layer) {
  AnnotationScope scope(channel_.get(), "Last Layer");

  // Compute last layer degree.
  const size_t last_layer_basis_index = Sum(params_->fri_step_list);
  const Coset lde_domain = params_->GetCosetForLayer(last_layer_basis_index);
  std::unique_ptr<LdeManager<ExtensionFieldElement>> lde_manager =
      MakeLdeManager<ExtensionFieldElement>(lde_domain, /*eval_in_natural_order=*/false);
  std::vector<ExtensionFieldElement> last_layer_evaluations = last_layer->GetLayer();
  lde_manager->AddEvaluation(std::move(last_layer_evaluations));
  const int64_t degree = lde_manager->GetEvaluationDegree(0);

  // If the original witness was of the correct degree, the last layer should be of degree <
  // last_layer_degree_bound.
  const uint64_t degree_bound = params_->last_layer_degree_bound;
  ASSERT_RELEASE(
      degree < (int64_t)degree_bound, "Last FRI layer is of degree: " + std::to_string(degree) +
                                          ". Expected degree < " + std::to_string(degree_bound) +
                                          ".");

  const auto coefficients = lde_manager->GetCoefficients(0);
  channel_->SendFieldElementSpan(coefficients.subspan(0, degree_bound), "Coefficients");
}

void FriProver::ProveFri() {
  // Commitment phase.
  {
    AnnotationScope scope(channel_.get(), "Commitment");
    MaybeOwnedPtr<const FriLayer> last_layer = CommitmentPhase();
    SendLastLayer(std::move(last_layer));
  }

  // Query phase.
  std::vector<uint64_t> queries = ChooseQueryIndices(
      channel_.get(), params_->GetLayerDomainSize(params_->fri_step_list.at(0)), params_->n_queries,
      params_->proof_of_work_bits);

  // Note: following this line, the verifier must not send randomness to the prover.

  channel_->BeginQueryPhase();

  // Decommitment phase.
  AnnotationScope scope(channel_.get(), "Decommitment");

  ProfilingBlock profiling_block("FRI response generation");
  size_t layer_num = 0;
  for (auto& layer : committed_layers_) {
    AnnotationScope scope(channel_.get(), "Layer " + std::to_string(layer_num++));
    layer->Decommit(queries);
  }
}

}  // namespace starkware
