#ifndef STARKWARE_COMMITMENT_SCHEME_TABLE_VERIFIER_MOCK_H_
#define STARKWARE_COMMITMENT_SCHEME_TABLE_VERIFIER_MOCK_H_

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "gmock/gmock.h"

#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

class TableVerifierMock : public TableVerifier<ExtensionFieldElement> {
 public:
  MOCK_METHOD0(ReadCommitment, void());
  MOCK_METHOD2(
      Query, std::map<RowCol, ExtensionFieldElement>(
                 const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries));
  MOCK_METHOD1(VerifyDecommitment, bool(const std::map<RowCol, ExtensionFieldElement>& data));
};

/*
  A factory that holds several mocks and each time it is called returns the next mock.
  This class can be used to test functions that get a TableVerifierFactory.
*/
// Disable linter since the destructor is effectively empty and therefore copy constructor and
// operator=() are not necessary.
template <typename FieldElementT>
class TableVerifierMockFactory {  // NOLINT
 public:
  explicit TableVerifierMockFactory(std::vector<std::pair<uint64_t, size_t>> expected_params)
      : expected_params_(std::move(expected_params)) {
    for (uint64_t i = 0; i < expected_params_.size(); ++i) {
      mocks_.push_back(std::make_unique<testing::StrictMock<TableVerifierMock>>());
    }
  }

  ~TableVerifierMockFactory() { EXPECT_EQ(mocks_.size(), cur_index_); }

  TableVerifierFactory<FieldElementT> AsFactory() {
    return [this](uint64_t n_rows, size_t n_columns) {
      ASSERT_RELEASE(
          cur_index_ < mocks_.size(),
          "Operator() of TableVerifierMockFactory was called too many times.");
      EXPECT_EQ(expected_params_[cur_index_], std::make_pair(n_rows, n_columns));
      return std::move(mocks_[cur_index_++]);
    };
  }

  /*
    Returns the mock at the given index.
    Should not be used after AsFactory() was called.
  */
  TableVerifierMock& operator[](size_t index) {
    ASSERT_RELEASE(
        cur_index_ == 0, "TableVerifierMockFactory: Operator[] cannot be used after AsFactory().");
    return *mocks_[index];
  }

 private:
  std::vector<std::pair<uint64_t, size_t>> expected_params_;
  std::vector<std::unique_ptr<testing::StrictMock<TableVerifierMock>>> mocks_;
  size_t cur_index_ = 0;
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_TABLE_VERIFIER_MOCK_H_
