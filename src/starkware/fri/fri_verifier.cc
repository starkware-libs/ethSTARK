#include "starkware/fri/fri_verifier.h"

#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "starkware/algebra/lde/lde_manager.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/commitment_scheme/table_impl_details.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

void FriVerifier::Init() {
  eval_points_.reserve(n_layers_ - 1);
  table_verifiers_.reserve(n_layers_ - 1);
  query_results_.reserve(params_->n_queries);
}

void FriVerifier::CommitmentPhase() {
  size_t basis_index = 0;
  for (size_t i = 0; i < n_layers_; i++) {
    size_t cur_fri_step = params_->fri_step_list[i];
    AnnotationScope scope(channel_.get(), "Layer " + std::to_string(i + 1));
    basis_index += cur_fri_step;
    if (i == 0) {
      if (params_->fri_step_list[0] != 0) {
        first_eval_point_ = channel_->GetAndSendRandomFieldElement("Evaluation point");
      }
    } else {
      eval_points_.push_back(channel_->GetAndSendRandomFieldElement("Evaluation point"));
    }
    if (i < n_layers_ - 1) {
      size_t coset_size = Pow2(params_->fri_step_list[i + 1]);
      std::unique_ptr<TableVerifier<ExtensionFieldElement>> commitment_scheme =
          (*table_verifier_factory_)(
              SafeDiv(params_->GetLayerDomainSize(basis_index), coset_size), coset_size);
      commitment_scheme->ReadCommitment();
      table_verifiers_.push_back(std::move(commitment_scheme));
    }
  }
}

void FriVerifier::ReadLastLayerCoefficients() {
  AnnotationScope scope(channel_.get(), "Last Layer");
  const size_t fri_step_sum = Sum(params_->fri_step_list);
  const uint64_t last_layer_size = params_->GetLayerDomainSize(fri_step_sum);

  ASSERT_RELEASE(
      params_->last_layer_degree_bound <= last_layer_size,
      "last_layer_degree_bound (" + std::to_string(params_->last_layer_degree_bound) +
          ") must be <= last_layer_size (" + std::to_string(last_layer_size) + ").");

  // Allocate a vector of zeros of size last_layer_size and fill the first last_layer_degree_bound
  // elements.
  std::vector<ExtensionFieldElement> last_layer_coefficients_vector(
      last_layer_size, ExtensionFieldElement::Zero());
  channel_->ReceiveFieldElementSpan(
      gsl::make_span(last_layer_coefficients_vector).subspan(0, params_->last_layer_degree_bound),
      "Coefficients");

  size_t last_layer_basis_index = Sum(params_->fri_step_list);
  const Coset lde_domain = params_->GetCosetForLayer(last_layer_basis_index);

  std::unique_ptr<LdeManager<ExtensionFieldElement>> last_layer_lde =
      MakeLdeManager<ExtensionFieldElement>(lde_domain, /*eval_in_natural_order=*/false);

  last_layer_lde->AddFromCoefficients(
      gsl::span<const ExtensionFieldElement>(last_layer_coefficients_vector));
  expected_last_layer_ = ExtensionFieldElement::UninitializedVector(last_layer_size);

  last_layer_lde->EvalOnCoset(
      lde_domain.Offset(), std::vector<gsl::span<ExtensionFieldElement>>{*expected_last_layer_});
}

void FriVerifier::VerifyFirstLayer() {
  AnnotationScope scope(channel_.get(), "Layer 0");
  const size_t first_fri_step = params_->fri_step_list.at(0);
  std::vector<uint64_t> first_layer_queries =
      fri::details::SecondLayerQueriesToFirstLayerQueries(query_indices_, first_fri_step);
  const std::vector<ExtensionFieldElement> first_layer_results =
      (*first_layer_queries_callback_)(first_layer_queries);
  ASSERT_RELEASE(
      first_layer_results.size() == first_layer_queries.size(),
      "Returned number of queries does not match the number sent.");
  const size_t first_layer_coset_size = Pow2(first_fri_step);
  for (size_t i = 0; i < first_layer_queries.size(); i += first_layer_coset_size) {
    query_results_.push_back(fri::details::ApplyFriLayers(
        gsl::make_span(first_layer_results).subspan(i, first_layer_coset_size), *first_eval_point_,
        *params_, 0, first_layer_queries[i]));
  }
}

void FriVerifier::VerifyInnerLayers() {
  const size_t first_fri_step = params_->fri_step_list.at(0);
  size_t basis_index = 0;
  for (size_t i = 0; i < n_layers_ - 1; ++i) {
    AnnotationScope scope(channel_.get(), "Layer " + std::to_string(i + 1));

    const size_t cur_fri_step = params_->fri_step_list[i + 1];
    basis_index += params_->fri_step_list[i];

    std::set<RowCol> layer_data_queries, layer_integrity_queries;
    fri::details::NextLayerDataAndIntegrityQueries(
        query_indices_, *params_, i + 1, &layer_data_queries, &layer_integrity_queries);
    // Collect results for data queries.
    std::map<RowCol, ExtensionFieldElement> to_verify =
        table_verifiers_[i]->Query(layer_data_queries, layer_integrity_queries);
    for (size_t j = 0; j < query_results_.size(); ++j) {
      const uint64_t query_index = query_indices_[j] >> (basis_index - first_fri_step);
      const RowCol& query_loc = fri::details::GetTableProverRowCol(query_index, cur_fri_step);
      to_verify.insert(std::make_pair(query_loc, query_results_[j]));
    }
    // Compute next layer.
    const ExtensionFieldElement& eval_point = eval_points_[i];
    for (size_t j = 0; j < query_results_.size(); ++j) {
      const size_t coset_size = Pow2(cur_fri_step);
      std::vector<ExtensionFieldElement> coset_elements;
      coset_elements.reserve(coset_size);
      const uint64_t coset_start = fri::details::GetTableProverRow(
          query_indices_[j] >> (basis_index - first_fri_step), cur_fri_step);
      for (size_t k = 0; k < coset_size; k++) {
        coset_elements.push_back(to_verify.at(RowCol(coset_start, k)));
      }
      query_results_[j] = fri::details::ApplyFriLayers(
          coset_elements, eval_point, *params_, i + 1, coset_start * Pow2(cur_fri_step));
    }

    ASSERT_RELEASE(
        table_verifiers_[i]->VerifyDecommitment(to_verify),
        "Layer " + std::to_string(i) + " failed decommitment.");
  }
}

void FriVerifier::VerifyLastLayer() {
  const size_t first_fri_step = params_->fri_step_list.at(0);
  const size_t fri_step_sum = Sum(params_->fri_step_list);

  ASSERT_RELEASE(
      expected_last_layer_.has_value(), "ReadLastLayer() must be called before VerifyLastLayer().");

  AnnotationScope scope(channel_.get(), "Last Layer");

  for (size_t j = 0; j < query_results_.size(); ++j) {
    const uint64_t query_index = query_indices_[j] >> (fri_step_sum - first_fri_step);
    const ExtensionFieldElement expected_value = expected_last_layer_->at(query_index);
    ASSERT_RELEASE(
        query_results_[j] == expected_value,
        "FRI query #" + std::to_string(j) +
            " is not consistent with the coefficients of the last layer.");
  }
}

void FriVerifier::VerifyFri() {
  Init();
  // Commitment phase.
  {
    AnnotationScope scope(channel_.get(), "Commitment");
    CommitmentPhase();
    ReadLastLayerCoefficients();
  }

  // Query phase.
  query_indices_ = fri::details::ChooseQueryIndices(
      channel_.get(), params_->GetLayerDomainSize(params_->fri_step_list.at(0)), params_->n_queries,
      params_->proof_of_work_bits);
  // Verifier cannot send randomness to the prover after the following line.
  channel_->BeginQueryPhase();

  // Decommitment phase.
  AnnotationScope scope(channel_.get(), "Decommitment");

  VerifyFirstLayer();

  // Inner layers.
  VerifyInnerLayers();

  // Last layer.
  VerifyLastLayer();
}

}  // namespace starkware
