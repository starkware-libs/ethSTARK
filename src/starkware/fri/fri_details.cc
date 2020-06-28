#include "starkware/fri/fri_details.h"

#include <algorithm>
#include <utility>

#include "starkware/algebra/lde/lde_manager.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/fri/fri_folder.h"
#include "starkware/math/math.h"

namespace starkware {
namespace fri {
namespace details {

ExtensionFieldElement ApplyFriLayers(
    gsl::span<const ExtensionFieldElement> elements,
    const std::optional<ExtensionFieldElement>& eval_point, const FriParameters& params,
    size_t layer_num, uint64_t first_element_index) {
  std::optional<ExtensionFieldElement> curr_eval_point = eval_point;
  // Find the first relevant basis for the requested layer.
  size_t cumulative_fri_step = 0;
  for (size_t i = 0; i < layer_num; ++i) {
    cumulative_fri_step += params.fri_step_list[i];
  }

  const size_t layer_fri_step = params.fri_step_list[layer_num];
  ASSERT_RELEASE(
      elements.size() == Pow2(layer_fri_step),
      "Number of elements is not consistent with the fri_step parameter.");
  std::vector<ExtensionFieldElement> cur_layer(elements.begin(), elements.end());
  for (size_t basis_index = cumulative_fri_step; basis_index < cumulative_fri_step + layer_fri_step;
       basis_index++) {
    ASSERT_RELEASE(curr_eval_point.has_value(), "Evaluation point doesn't have a value.");

    // Apply NextLayerElementFromTwoPreviousLayerElements() on pairs of field elements to compute
    // the next inner layer.
    const Coset basis = params.GetCosetForLayer(basis_index);
    std::vector<ExtensionFieldElement> next_layer;
    next_layer.reserve(cur_layer.size() / 2);
    for (size_t j = 0; j < cur_layer.size(); j += 2) {
      next_layer.push_back(FriFolder::NextLayerElementFromTwoPreviousLayerElements(
          cur_layer[j], cur_layer[j + 1], *curr_eval_point,
          basis.AtBitReversed(first_element_index + j)));
    }

    // Update the variables for the next iteration.
    cur_layer = std::move(next_layer);
    curr_eval_point = (*curr_eval_point) * (*curr_eval_point);
    first_element_index /= 2;
  }

  ASSERT_RELEASE(cur_layer.size() == 1, "Expected number of elements to be one.");
  return cur_layer[0];
}

std::vector<uint64_t> SecondLayerQueriesToFirstLayerQueries(
    const std::vector<uint64_t>& query_indices, size_t first_fri_step) {
  std::vector<uint64_t> first_layer_queries;
  const size_t first_layer_coset_size = Pow2(first_fri_step);
  first_layer_queries.reserve(query_indices.size() * first_layer_coset_size);
  for (uint64_t idx : query_indices) {
    for (uint64_t i = idx * first_layer_coset_size; i < (idx + 1) * first_layer_coset_size; ++i) {
      first_layer_queries.push_back(i);
    }
  }
  return first_layer_queries;
}

void NextLayerDataAndIntegrityQueries(
    const std::vector<uint64_t>& query_indices, const FriParameters& params, size_t layer_num,
    std::set<RowCol>* data_queries, std::set<RowCol>* integrity_queries) {
  // cumulative_fri_step is the sum of fri_step starting from the second layer and up to the
  // requested layer. It allows us to compute the indices of the queries in the requested layer,
  // given the indices of the second layer.
  size_t cumulative_fri_step = 0;
  for (size_t i = 1; i < layer_num; ++i) {
    cumulative_fri_step += params.fri_step_list[i];
  }
  const size_t layer_fri_step = params.fri_step_list[layer_num];

  for (uint64_t idx : query_indices) {
    integrity_queries->insert(GetTableProverRowCol(idx >> cumulative_fri_step, layer_fri_step));
  }
  for (uint64_t idx : query_indices) {
    // Find the first element of the coset: Divide idx by 2^cumulative_fri_step to find the query
    // location in the current layer, then clean the lower bits to get the first query in the coset.
    const uint64_t coset_row = GetTableProverRow(idx >> cumulative_fri_step, layer_fri_step);
    for (size_t coset_col = 0; coset_col < Pow2(layer_fri_step); coset_col++) {
      RowCol query{coset_row, coset_col};
      // Add the query to data_queries only if it is not an integrity query.
      if (integrity_queries->count(query) == 0) {
        data_queries->insert(query);
      }
    }
  }
}

std::vector<uint64_t> ChooseQueryIndices(
    Channel* channel, uint64_t domain_size, size_t n_queries, size_t proof_of_work_bits) {
  // Proof of work right before randomizing queries.
  channel->ApplyProofOfWork(proof_of_work_bits);

  AnnotationScope scope(channel, "QueryIndices");

  std::vector<uint64_t> query_indices;
  query_indices.reserve(n_queries);
  for (size_t i = 0; i < n_queries; ++i) {
    query_indices.push_back(channel->GetRandomNumberFromVerifier(domain_size, std::to_string(i)));
  }
  std::sort(query_indices.begin(), query_indices.end());
  return query_indices;
}

}  // namespace details
}  // namespace fri
}  // namespace starkware
