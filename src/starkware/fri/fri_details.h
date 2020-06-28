#ifndef STARKWARE_FRI_FRI_DETAILS_H_
#define STARKWARE_FRI_FRI_DETAILS_H_

#include <memory>
#include <set>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/channel.h"
#include "starkware/commitment_scheme/row_col.h"
#include "starkware/fri/fri_parameters.h"

namespace starkware {
namespace fri {
namespace details {

/*
  Computes the element from the next FRI layer, given the corresponding coset from the current
  layer.
  For example, if fri_step_list[layer_num] = 1, this function behaves the same as
  NextLayerElementFromTwoPreviousLayerElements().
*/
ExtensionFieldElement ApplyFriLayers(
    gsl::span<const ExtensionFieldElement> elements,
    const std::optional<ExtensionFieldElement>& eval_point, const FriParameters& params,
    size_t layer_num, uint64_t first_element_index);

/*
  Given query indices that refer to FRI's second layer, compute the indices of the cosets in the
  first layer.
*/
std::vector<uint64_t> SecondLayerQueriesToFirstLayerQueries(
    const std::vector<uint64_t>& query_indices, size_t first_fri_step);

/*
  Given the query indices (of FRI's second layer), we compute the data queries and integrity queries
  for the next layer of FRI.
  Data queries are queries whose data needs to go over the channel.
  Integrity queries are ones that each party can compute based on previously known information.

  For example, if fri_step of the corresponding layer is 3, then the size of the coset is 8. The
  verifier will be able to compute one element (integrity query) and the other 7 will be sent in the
  channel (data queries).

  Note: The two resulting sets are disjoint.
*/
void NextLayerDataAndIntegrityQueries(
    const std::vector<uint64_t>& query_indices, const FriParameters& params, size_t layer_num,
    std::set<RowCol>* data_queries, std::set<RowCol>* integrity_queries);

/*
  Returns n_queries random indices from a domain of size domain_size.
  The function is used by both the prover and the verifier, and applies proof_of_work_bits to the
  corresponding channel.
*/
std::vector<uint64_t> ChooseQueryIndices(
    Channel* channel, uint64_t domain_size, size_t n_queries, size_t proof_of_work_bits);

// Given the query index in the layer (1D), calculate the cell position in the 2D table
// according to coset size (always power of 2).
// fri_step is a log2 of coset size (row_size).

/*
  Logic: query_index >> fri_step == query_index / Pow2(fri_step) == query_index / row_size.
*/
inline uint64_t GetTableProverRow(const uint64_t query_index, const size_t fri_step) {
  return query_index >> fri_step;
}

/*
  Logic: query_index & (Pow2(fri_step) - 1) == query_index % row_size
  (Pow2(fri_step) - 1) is a mask of 1s to the row_size.
*/
inline uint64_t GetTableProverCol(const uint64_t query_index, const size_t fri_step) {
  return query_index & (Pow2(fri_step) - 1);
}

inline RowCol GetTableProverRowCol(const uint64_t query_index, const size_t fri_step) {
  return {GetTableProverRow(query_index, fri_step), GetTableProverCol(query_index, fri_step)};
}

}  // namespace details
}  // namespace fri
}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_DETAILS_H_
