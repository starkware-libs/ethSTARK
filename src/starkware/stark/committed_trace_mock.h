#ifndef STARKWARE_STARK_COMMITTED_TRACE_MOCK_H_
#define STARKWARE_STARK_COMMITTED_TRACE_MOCK_H_

#include <tuple>
#include <utility>
#include <vector>

#include "gmock/gmock.h"

#include "starkware/stark/committed_trace.h"

namespace starkware {

template <typename FieldElementT>
class CommittedTraceProverMock : public CommittedTraceProverBase<FieldElementT> {
 public:
  MOCK_CONST_METHOD0(NumColumns, size_t());
  MOCK_METHOD0_T(GetLde, CachedLdeManager<FieldElementT>*());

  /*
    Workaround: GMock has a problem with rvalue reference parameter (&&). So, we implement it and
    pass execution to Commit_rvr (rvr stands for rvalue reference).
  */
  void Commit(
      TraceBase<FieldElementT>&& trace, const Coset& trace_domain,
      bool eval_in_natural_order) override {
    Commit_rvr(trace, trace_domain, eval_in_natural_order);
  }
  MOCK_METHOD3_T(Commit_rvr, void(const TraceBase<FieldElementT>&, const Coset&, bool));
  MOCK_CONST_METHOD1(
      DecommitQueries, void(gsl::span<const std::tuple<uint64_t, uint64_t, size_t>>));
  MOCK_CONST_METHOD3(
      EvalMaskAtPoint, void(
                           gsl::span<const std::pair<int64_t, uint64_t>>,
                           const ExtensionFieldElement&, gsl::span<ExtensionFieldElement>));
  MOCK_METHOD0(FinalizeEval, void());
};

template <typename FieldElementT>
class CommittedTraceVerifierMock : public CommittedTraceVerifierBase<FieldElementT> {
 public:
  MOCK_CONST_METHOD0(NumColumns, size_t());
  MOCK_METHOD0(ReadCommitment, void());
  MOCK_CONST_METHOD1_T(
      VerifyDecommitment,
      std::vector<FieldElementT>(gsl::span<const std::tuple<uint64_t, uint64_t, size_t>>));
};

}  // namespace starkware

#endif  // STARKWARE_STARK_COMMITTED_TRACE_MOCK_H_
