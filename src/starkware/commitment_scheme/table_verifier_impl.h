#ifndef STARKWARE_COMMITMENT_SCHEME_TABLE_VERIFIER_IMPL_H_
#define STARKWARE_COMMITMENT_SCHEME_TABLE_VERIFIER_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/commitment_scheme.h"
#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

template <typename FieldElementT>
class TableVerifierImpl : public TableVerifier<FieldElementT> {
 public:
  TableVerifierImpl(
      size_t n_columns, MaybeOwnedPtr<CommitmentSchemeVerifier> commitment_scheme,
      VerifierChannel* channel)
      : n_columns_(n_columns),
        commitment_scheme_(std::move(commitment_scheme)),
        channel_(channel) {}

  void ReadCommitment() override;

  std::map<RowCol, FieldElementT> Query(
      const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries) override;

  bool VerifyDecommitment(const std::map<RowCol, FieldElementT>& all_rows_data) override;

 private:
  size_t n_columns_;
  MaybeOwnedPtr<CommitmentSchemeVerifier> commitment_scheme_;
  VerifierChannel* channel_;
};

}  // namespace starkware

#include "starkware/commitment_scheme/table_verifier_impl.inl"

#endif  // STARKWARE_COMMITMENT_SCHEME_TABLE_VERIFIER_IMPL_H_
