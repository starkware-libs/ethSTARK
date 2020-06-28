#include "starkware/commitment_scheme/table_prover_impl.h"

#include <cstddef>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/channel/prover_channel.h"
#include "starkware/channel/prover_channel_mock.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/commitment_scheme_mock.h"
#include "starkware/commitment_scheme/merkle/merkle_commitment_scheme.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {
namespace {

using testing::ElementsAreArray;
using testing::HasSubstr;
using testing::InSequence;
using testing::Property;
using testing::Return;
using testing::StrictMock;
using testing::UnorderedElementsAre;

TEST(RowCol, RowColToString) {
  RowCol zero(0, 0);
  std::stringstream ss;
  ss << zero;
  EXPECT_EQ("(0, 0)", ss.str());
  EXPECT_EQ("(0, 0)", zero.ToString());
  std::stringstream ss2;
  RowCol r(99, 12345678);
  ss2 << r;
  EXPECT_EQ("(99, 12345678)", ss2.str());
  EXPECT_EQ("(99, 12345678)", r.ToString());
}

/*
  Generates random data for tests. We use the same public input for all the tests in this file since
  it's irrelevant when testing table commitment.
*/
std::vector<std::vector<BaseFieldElement>> RandomData(
    Prng* prng, uint64_t n_rows, size_t n_columns) {
  std::vector<std::vector<BaseFieldElement>> columns;
  for (size_t col = 0; col < n_columns; ++col) {
    std::vector<BaseFieldElement> column;
    for (size_t row = 0; row < n_rows; ++row) {
      column.push_back(BaseFieldElement::RandomElement(prng));
    }
    columns.push_back(std::move(column));
  }
  return columns;
}

class TableProverImplTest : public testing::Test {
 protected:
  TableProverImplTest()
      : table_prover_(n_columns_, UseOwned(&commitment_scheme_prover_), &prover_channel_),
        columns_(RandomData(&prng_, n_segments_ * n_rows_per_segment_, n_columns_)) {}

  /*
    Allows separating the test to several parts.
    It makes sure that expectations that were set in one part are indeed called in this part.
    This helps in understanding what went wrong if the test fails and guarantees a stricter test.
  */
  void ClearExpectations() {
    testing::Mock::VerifyAndClearExpectations(&prover_channel_);
    testing::Mock::VerifyAndClearExpectations(&commitment_scheme_prover_);
    EXPECT_CALL(commitment_scheme_prover_, SegmentLengthInElements())
        .WillRepeatedly(Return(n_rows_per_segment_));
  }

  void CallAddSegmentForCommitment() {
    ClearExpectations();
    for (uint64_t segment_idx = 0; segment_idx < n_segments_; ++segment_idx) {
      // Every call to AddSegmentForCommitment() is expected to call
      // commitment_scheme_prover_.AddSegmentForCommitment().
      EXPECT_CALL(
          commitment_scheme_prover_,
          AddSegmentForCommitment(
              Property(
                  &gsl::span<const std::byte>::size,
                  n_rows_per_segment_ * n_columns_ * BaseFieldElement::SizeInBytes()),
              segment_idx));

      std::vector<gsl::span<const BaseFieldElement>> segment_span;
      segment_span.reserve(n_columns_);
      for (const auto& column : columns_) {
        segment_span.push_back(
            gsl::make_span(column).subspan(segment_idx * n_rows_per_segment_, n_rows_per_segment_));
      }
      table_prover_.AddSegmentForCommitment(segment_span, segment_idx, 1);
    }
  }

  void CallCommit() {
    // Commit() is expected to call ComputeCommitment() on the merkle tree and then send the result
    // on the channel.
    ClearExpectations();
    EXPECT_CALL(commitment_scheme_prover_, Commit());
    table_prover_.Commit();
  }

  const uint64_t n_segments_ = 8;
  const uint64_t n_rows_per_segment_ = 16;
  const uint64_t n_columns_ = 3;
  Prng prng_;
  StrictMock<ProverChannelMock> prover_channel_;
  StrictMock<CommitmentSchemeProverMock> commitment_scheme_prover_;
  TableProverImpl<BaseFieldElement> table_prover_;

  std::vector<std::vector<BaseFieldElement>> columns_;
};

/*
  Tests TableProverImpl by mocking the channel and the inner commitment scheme.
*/
TEST_F(TableProverImplTest, BasicFlow) {
  CallAddSegmentForCommitment();

  CallCommit();

  // Call StartDecommitmentPhase().
  // data_queries are {4, 0}, {4, 1}, {93, 1}.
  // integrity_queries are {93, 2}, {8, 0}, {8, 1}, {8, 2}.
  // To test that the rows requested by the inner commitment_scheme_prover_ are returned, we
  // mock commitment_scheme_prover_.StartDecommitmentPhase() to ask for rows 1 and 20.
  ClearExpectations();
  EXPECT_CALL(commitment_scheme_prover_, StartDecommitmentPhase(UnorderedElementsAre(4, 8, 93)))
      .WillOnce(Return(std::vector<uint64_t>{1, 20}));
  const std::vector<uint64_t> expected_requested_rows = {4, 8, 93, 1, 20};
  EXPECT_THAT(
      table_prover_.StartDecommitmentPhase(
          {{4, 0}, {4, 1}, {93, 1}}, {{93, 2}, {8, 0}, {8, 1}, {8, 2}}),
      ElementsAreArray(expected_requested_rows));

  // Call Decommit().
  // Decommit() is expected to send all the field elements that participate in the mentioned rows
  // (4, 8, 93) except for integrity queries.
  // Then it is expected to call commitment_scheme_prover_.Decommit().
  ClearExpectations();
  {
    testing::InSequence dummy;
    EXPECT_CALL(prover_channel_, SendFieldElementImpl(columns_.at(0).at(4)));
    EXPECT_CALL(prover_channel_, SendFieldElementImpl(columns_.at(1).at(4)));
    EXPECT_CALL(prover_channel_, SendFieldElementImpl(columns_.at(2).at(4)));
    // Skip (0, 8, 0).
    // Skip (0, 8, 1).
    // Skip (0, 8, 2).
    EXPECT_CALL(prover_channel_, SendFieldElementImpl(columns_.at(0).at(93)));
    EXPECT_CALL(prover_channel_, SendFieldElementImpl(columns_.at(1).at(93)));
    // Skip (5, 13, 2).
    const size_t elements_data_expected_size = 2 * n_columns_ * BaseFieldElement::SizeInBytes();
    EXPECT_CALL(
        commitment_scheme_prover_,
        Decommit(Property(&gsl::span<const std::byte>::size, elements_data_expected_size)));
  }

  // Fill a vector with the returned rows.
  std::vector<std::vector<BaseFieldElement>> elements_data;
  for (const auto& column : columns_) {
    std::vector<BaseFieldElement> res;
    res.reserve(expected_requested_rows.size());
    for (size_t row : expected_requested_rows) {
      res.push_back(column[row]);
    }
    elements_data.push_back(std::move(res));
  }
  table_prover_.Decommit(
      std::vector<gsl::span<const BaseFieldElement>>(elements_data.begin(), elements_data.end()));
}

TEST_F(TableProverImplTest, QuerySetsMustBeDisjoint) {
  CallAddSegmentForCommitment();
  CallCommit();
  EXPECT_ASSERT(
      table_prover_.StartDecommitmentPhase({{2, 0}, {3, 0}}, {{1, 1}, {2, 0}}),
      HasSubstr("must be disjoint"));
}

}  // namespace
}  // namespace starkware
