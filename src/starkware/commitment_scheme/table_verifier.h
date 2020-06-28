#ifndef STARKWARE_COMMITMENT_SCHEME_TABLE_VERIFIER_H_
#define STARKWARE_COMMITMENT_SCHEME_TABLE_VERIFIER_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "starkware/commitment_scheme/table_prover.h"

namespace starkware {

template <typename FieldElementT>
class TableVerifier {
 public:
  virtual ~TableVerifier() = default;

  /*
    Reads the initial commitment (e.g., Merkle root).
  */
  virtual void ReadCommitment() = 0;

  /*
    Returns query results from the channel.
    The input to this function is data queries (i.e. queries the verifier does not know the answer
    to) and integrity queries (i.e. queries for which the verifier can compute the answer).
    The resulting map contains the responses to the data queries, as well as the "clues", which are
    all the cells that share a row with some integrity/data query, but are not such themselves.
  */
  virtual std::map<RowCol, FieldElementT> Query(
      const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries) = 0;

  /*
    Verifies that the given field elements are indeed the ones
    committed to by the prover, against the commitment obtained by ReadCommitment(). The key set of
    data should be the union of the data queries and the integrity queries.
  */
  virtual bool VerifyDecommitment(const std::map<RowCol, FieldElementT>& all_rows_data) = 0;
};

/*
  A factory of TableVerifer is a function that creates an instance of a subclass of
  TableVerifier.
  The factory notion here is used for two purposes:
  1. Allow the caller of VerifyFri to set the type of table commitment used.
  2. Use mocks for testing.
*/
template <typename FieldElementT>
using TableVerifierFactory =
    std::function<std::unique_ptr<TableVerifier<FieldElementT>>(uint64_t n_rows, size_t n_columns)>;

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_TABLE_VERIFIER_H_
