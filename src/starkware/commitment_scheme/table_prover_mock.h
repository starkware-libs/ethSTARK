#ifndef STARKWARE_COMMITMENT_SCHEME_TABLE_PROVER_MOCK_H_
#define STARKWARE_COMMITMENT_SCHEME_TABLE_PROVER_MOCK_H_

#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "gmock/gmock.h"

#include "starkware/commitment_scheme/table_prover.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

template <typename FieldElementT>
class TableProverMock : public TableProver<FieldElementT> {
 public:
  MOCK_METHOD3_T(
      AddSegmentForCommitment, void(
                                   const std::vector<gsl::span<const FieldElementT>>& segment,
                                   size_t segment_index, size_t n_interleaved_columns));
  MOCK_METHOD0(Commit, void());
  MOCK_METHOD2(
      StartDecommitmentPhase,
      std::vector<uint64_t>(
          const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries));
  MOCK_METHOD1_T(Decommit, void(gsl::span<const gsl::span<const FieldElementT>> elements_data));
};

/*
  A factory that holds several mocks and each time it is called returns the next mock.
  This class can be used to test functions or classes that get a TableProverFactory.

  Usage: To allow testing that the function TestedFunction(const TableProverMockFactory&) performs
  the following steps:
      1. TableProverFactory(n_segments=1, n_rows_per_segment=32, n_columns=8)
      2. TableProverFactory(n_segments=1, n_rows_per_segment=16, n_columns=2)
      3. AddSegmentForCommitment on the first table prover with arguments (segment=*anything*,
         segment_index=0, n_interleaved_columns=8).
      4. Commit the segment added to the first table prover.
      5. AddSegmentForCommitment on the second table prover with arguments (segment=*anything*,
         segment_index=0, n_interleaved_columns=2).
      6. Commit the segment added to the second table prover.
  Use the following code:
      1+2. TableProverMockFactory<ExtensionFieldElement> table_prover_factory(
              {std::make_tuple(1, 32, 8), std::make_tuple(1, 16, 2)});
      3. EXPECT_CALL(table_prover_factory[0], AddSegmentForCommitment(_, 0, 8));
      4. EXPECT_CALL(table_prover_factory[0], Commit());
      5. EXPECT_CALL(table_prover_factory[1], AddSegmentForCommitment(_, 0, 2));
      6. EXPECT_CALL(table_prover_factory[1], Commit());
  After the prepration, call the tested function or class and continue your tests:
      TestedFunction(...,table_prover_factory.AsFactory(),...);
*/
// Disable linter since the destructor is effectively empty and therefore copy constructor and
// operator=() are not necessary.
template <typename FieldElementT>
class TableProverMockFactory {  // NOLINT
 public:
  explicit TableProverMockFactory(
      std::vector<std::tuple<uint64_t, uint64_t, size_t>> expected_params)
      : expected_params_(std::move(expected_params)) {
    for (uint64_t i = 0; i < expected_params_.size(); ++i) {
      mocks_.push_back(std::make_unique<testing::StrictMock<TableProverMock<FieldElementT>>>());
    }
  }

  ~TableProverMockFactory() { EXPECT_EQ(mocks_.size(), cur_index_); }

  TableProverFactory<FieldElementT> AsFactory() {
    return [this](size_t n_segments, uint64_t n_rows_per_segment, size_t n_columns) {
      ASSERT_RELEASE(
          cur_index_ < mocks_.size(),
          "Operator() of TableProverMockFactory was called too many times.");
      EXPECT_EQ(
          expected_params_[cur_index_], std::make_tuple(n_segments, n_rows_per_segment, n_columns));
      return std::move(mocks_[cur_index_++]);
    };
  }

  /*
    Returns the mock at the given index.
    Should not be used after AsFactory() was called.
  */
  TableProverMock<FieldElementT>& operator[](size_t index) {
    ASSERT_RELEASE(
        cur_index_ == 0, "TableProverMockFactory: Operator[] cannot be used after AsFactory().");
    return *mocks_[index];
  }

 private:
  std::vector<std::unique_ptr<testing::StrictMock<TableProverMock<FieldElementT>>>> mocks_;
  std::vector<std::tuple<uint64_t, uint64_t, size_t>> expected_params_;
  size_t cur_index_ = 0;
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_TABLE_PROVER_MOCK_H_
