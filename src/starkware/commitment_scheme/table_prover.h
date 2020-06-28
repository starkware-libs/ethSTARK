#ifndef STARKWARE_COMMITMENT_SCHEME_TABLE_PROVER_H_
#define STARKWARE_COMMITMENT_SCHEME_TABLE_PROVER_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/commitment_scheme/row_col.h"

namespace starkware {

/*
  An interface for committing and decommitting a 2 dimensional array of field elements.
  Every row of the array is packed into one element in the parent commitment scheme.
  Therefore, the entire row must be revealed in order to reveal any cell in it.
*/
template <typename FieldElementT>
class TableProver {
 public:
  virtual ~TableProver() = default;

  /*
    Adds a segment of data to the commitment scheme.
    Every span in segment is composed of 'n_interleaved_columns' interleaved columns.

    For example, when n_columns = n_interleaved_columns = 2 and segment has only one span which is
    [a,b,c,d], then the first row will be [a,b] and the second will be [c,d].

      segment:
       _   _            table:
      |  a  |          _       _
      |  b  |   ==>   |  a , b  |
      |  c  |         |_ c , d _|
      |_ d _|

    And when segment has two spans which are [a,b] and [c,d], then the
    first row will be [a,c] and the second will be [b,d].

           segment:               table:
       _   _     _   _           _       _
      |  a  |   |  c  |         |  a , c  |
      |_ b _| , |_ d _|   ==>   |_ b , d _|.
  */
  virtual void AddSegmentForCommitment(
      const std::vector<gsl::span<const FieldElementT>>& segment, size_t segment_index,
      size_t n_interleaved_columns) = 0;

  /*
    Calls the previous function with n_interleaved_columns = 1 (default value).
  */
  void AddSegmentForCommitment(
      const std::vector<gsl::span<const FieldElementT>>& segment, size_t segment_index) {
    AddSegmentForCommitment(segment, segment_index, 1);
  }

  /*
    Commits to the data by sending the commitment on the channel.
    This function must be called after AddSegmentForCommitment() was called for all the segments.
  */
  virtual void Commit() = 0;

  /*
    Returns a list of rows whose values should be passed to Decommit(), for the given set of queries
    & integrity queries.

    * queries - a list of indices for which the actual data should be sent to the verifier with the
      decommitment.
    * integrity_queries - a list of indices for which the verifier can compute the data on its own,
      but wants to verify that its value are consistent with the commitment.
  */
  virtual std::vector<uint64_t> StartDecommitmentPhase(
      const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries) = 0;

  /*
    Finalizes the decommitment phase on the channel.
    elements_data is a 2D array (first indexed by column and then row). The rows should match the
    row indices requested by StartDecommitmentPhase() and contain the committed data.
  */
  virtual void Decommit(gsl::span<const gsl::span<const FieldElementT>> elements_data) = 0;
};

/*
  A factory of TableProver is a function that gets a size of the data to commit on, and creates an
  instance of a subclass of TableProver.
  The factory notion here is used for two purposes:
  1. Allow the caller of ProveFri to set the type of table commitment used.
  2. Use mocks for testing.
*/
template <typename FieldElementT>
using TableProverFactory = std::function<std::unique_ptr<TableProver<FieldElementT>>(
    size_t n_segments, uint64_t n_rows_per_segment, size_t n_columns)>;

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_TABLE_PROVER_H_
