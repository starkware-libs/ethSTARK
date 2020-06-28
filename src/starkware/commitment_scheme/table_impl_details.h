#ifndef STARKWARE_COMMITMENT_SCHEME_TABLE_IMPL_DETAILS_H_
#define STARKWARE_COMMITMENT_SCHEME_TABLE_IMPL_DETAILS_H_

#include <set>
#include <string>
#include <vector>

#include "starkware/channel/prover_channel.h"
#include "starkware/commitment_scheme/commitment_scheme.h"
#include "starkware/commitment_scheme/table_prover.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {
namespace table {
namespace details {

/*
  Given the cells of data queries and integrity queries, this function returns a set of all indices
  of rows that contain at least one query from the given sets.
*/
std::set<uint64_t> AllQueryRows(
    const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries);

/*
  Returns a set of cells pointing to the field elements that have to be transmitted to
  allow the verification of the queries. Those are all the cells that share a row with some
  integrity/data query, but are not integrity query cells themselves.
*/
std::set<RowCol> ElementsToBeTransmitted(
    size_t n_columns, const std::set<uint64_t>& all_query_rows,
    const std::set<RowCol>& integrity_queries);

std::string ElementDecommitAnnotation(const RowCol& row_col);

}  // namespace details
}  // namespace table
}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_TABLE_IMPL_DETAILS_H_
