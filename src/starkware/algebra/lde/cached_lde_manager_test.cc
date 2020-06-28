#include "starkware/algebra/lde/cached_lde_manager.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/lde/lde_manager_mock.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::_;
using testing::HasSubstr;
using testing::StrictMock;

/*
  Used to mock the output of LdeManager::EvalOnCoset().
  Assumes the mocked function's second argument (arg1) behaves like
  span<std::vector<FieldElementT>>, and writes to it.
  Example use: EXPECT_CALL(..., EvalOnCoset(...)).WillOnce(SetEvaluation(mock_evaluation)).
*/
ACTION_P(SetEvaluation, mock_evaluation) {
  auto& result = arg1;
  ASSERT_EQ(result.size(), mock_evaluation.size());

  for (size_t column_index = 0; column_index < result.size(); ++column_index) {
    ASSERT_EQ(result[column_index].size(), mock_evaluation[column_index].size());
    std::copy(
        mock_evaluation[column_index].begin(), mock_evaluation[column_index].end(),
        result[column_index].begin());
  }
}

class CachedLdeManagerTest : public ::testing::Test {
 public:
  CachedLdeManagerTest()
      : offsets_(prng_.RandomFieldElementVector<BaseFieldElement>(n_cosets_)),
        lde_manager_(coset_size_, BaseFieldElement::RandomElement(&prng_)),
        cached_lde_manager_(UseOwned(&lde_manager_), {offsets_.begin(), offsets_.end()}) {
    StartTest();
  }

 protected:
  const size_t coset_size_ = 16;
  const size_t n_cosets_ = 10;
  const size_t n_columns_ = 3;
  Prng prng_;
  const std::vector<BaseFieldElement> offsets_;
  StrictMock<LdeManagerMock> lde_manager_;
  CachedLdeManager<BaseFieldElement> cached_lde_manager_;

  /*
    Indices are: coset_index, column_index, point_index (index within coset).
  */
  std::vector<std::vector<std::vector<BaseFieldElement>>> evaluations_;

  /*
    Populates the mocked CachedLdeManager with random evaluations.
    Keeps the evaluations in evaluations_ for comparisons.
  */
  void StartTest();

  void TestEvalAtPointsResult(
      const std::vector<std::pair<size_t, uint64_t>>& coset_point_indices,
      const std::vector<std::vector<BaseFieldElement>>& outputs);
};

void CachedLdeManagerTest::StartTest() {
  // Test that correct variants of AddEvaluation are called.

  // AddEvaluation vector&& variant.
  EXPECT_CALL(lde_manager_, AddEvaluation_rvr(_)).Times(n_columns_ - 1);
  for (size_t i = 0; i < n_columns_ - 1; ++i) {
    auto evaluation = prng_.RandomFieldElementVector<BaseFieldElement>(coset_size_);
    cached_lde_manager_.AddEvaluation(std::move(evaluation));
  }
  // AddEvaluation span variant.
  auto evaluation = prng_.RandomFieldElementVector<BaseFieldElement>(coset_size_);
  gsl::span<const BaseFieldElement> evaluation_span(evaluation);
  EXPECT_CALL(lde_manager_, AddEvaluation(evaluation_span));
  cached_lde_manager_.AddEvaluation(evaluation_span);

  cached_lde_manager_.FinalizeAdding();

  // Evaluate cosets once.
  for (size_t coset_index = 0; coset_index < n_cosets_; ++coset_index) {
    std::vector<std::vector<BaseFieldElement>> coset_evaluation;
    BaseFieldElement offset = offsets_[coset_index];

    // Generate random coset values.
    for (size_t column_index = 0; column_index < n_columns_; ++column_index) {
      coset_evaluation.push_back(prng_.RandomFieldElementVector<BaseFieldElement>(coset_size_));
    }
    evaluations_.push_back(coset_evaluation);

    // Setup callback to return that coset.
    EXPECT_CALL(lde_manager_, EvalOnCoset(offset, _)).WillOnce(SetEvaluation(coset_evaluation));
    auto result = cached_lde_manager_.EvalOnCoset(coset_index);

    // Test that we got the correct evaluation.
    ASSERT_EQ(result->size(), n_columns_);
    for (size_t column_index = 0; column_index < n_columns_; ++column_index) {
      ASSERT_EQ((*result)[column_index], coset_evaluation[column_index]);
    }
  }

  cached_lde_manager_.FinalizeEvaluations();
}

TEST_F(CachedLdeManagerTest, EvalOnCoset_Cache) {
  // Evaluate again. Expect No additional computation, and same coset values as before.
  for (size_t coset_index = 0; coset_index < n_cosets_; ++coset_index) {
    EXPECT_CALL(lde_manager_, EvalOnCoset(offsets_[coset_index], _)).Times(0);
    auto result = cached_lde_manager_.EvalOnCoset(coset_index);
    ASSERT_EQ(result->size(), n_columns_);
    for (size_t column_index = 0; column_index < n_columns_; ++column_index) {
      ASSERT_EQ((*result)[column_index], evaluations_[coset_index][column_index]);
    }
  }
}

void CachedLdeManagerTest::TestEvalAtPointsResult(
    const std::vector<std::pair<size_t, uint64_t>>& coset_point_indices,
    const std::vector<std::vector<BaseFieldElement>>& outputs) {
  // Compare result.
  for (size_t column_index = 0; column_index < n_columns_; column_index++) {
    for (size_t i = 0; i < coset_point_indices.size(); ++i) {
      const auto& [coset_index, point_index] = coset_point_indices[i];
      ASSERT_EQ(outputs[column_index][i], evaluations_[coset_index][column_index][point_index]);
    }
  }
}

TEST_F(CachedLdeManagerTest, EvalAtPoints_Cache) {
  const size_t n_points = 30;
  std::vector<std::pair<size_t, uint64_t>> coset_point_indices;
  coset_point_indices.reserve(n_points);
  for (size_t i = 0; i < n_points; ++i) {
    coset_point_indices.emplace_back(
        prng_.UniformInt(0, 4), prng_.UniformInt(0, static_cast<int>(coset_size_ - 1)));
  }

  // Allocate outputs.
  std::vector<std::vector<BaseFieldElement>> outputs;
  for (size_t column_index = 0; column_index < n_columns_; ++column_index) {
    outputs.push_back(BaseFieldElement::UninitializedVector(n_points));
  }

  // Evaluate points.
  std::vector<gsl::span<BaseFieldElement>> outputs_spans = {outputs.begin(), outputs.end()};
  cached_lde_manager_.EvalAtPoints(coset_point_indices, outputs_spans);

  // Compare result.
  TestEvalAtPointsResult(coset_point_indices, outputs);
}

TEST_F(CachedLdeManagerTest, AddAfterEvalOnCoset) {
  auto evaluation = prng_.RandomFieldElementVector<BaseFieldElement>(coset_size_);

  EXPECT_ASSERT(
      cached_lde_manager_.AddEvaluation(std::move(evaluation)),
      HasSubstr("Cannot call AddEvaluation after"));
}

TEST_F(CachedLdeManagerTest, ComputeAfterFinalizeEvaluations) {
  const size_t n_points = 30;
  const auto points = prng_.RandomFieldElementVector<ExtensionFieldElement>(n_points);
  auto output = ExtensionFieldElement::UninitializedVector(n_points);

  EXPECT_ASSERT(
      cached_lde_manager_.EvalAtPointsNotCached(0, points, output),
      HasSubstr("FinalizeEvaluations()"));
}

}  // namespace
}  // namespace starkware
