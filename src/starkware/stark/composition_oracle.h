#ifndef STARKWARE_STARK_COMPOSITION_ORACLE_H_
#define STARKWARE_STARK_COMPOSITION_ORACLE_H_

#include <tuple>
#include <utility>
#include <vector>

#include "starkware/air/air.h"
#include "starkware/algebra/domains/evaluation_domain.h"
#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/composition_polynomial/composition_polynomial.h"
#include "starkware/stark/committed_trace.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

/*
  Given a committed trace (optionally with a committed composition trace), a mask and a composition
  polynomial, represents a virtual oracle of the composition polynomial.

  Composition_trace contains trace columns over the extension field. If composition_trace is
  provided, its columns are concatenated after the columns of the regular trace. A mask is a list
  of pairs (offset, column_index).
  The evaluation of the composition polynomial at point x, is a function of the elements
  (c_{column_index_i}[x + offset_i]) for i = 0..mask_size - 1.
  This will be called the mask (over the columns c_*) at point x. To decommit at point x, the
  virtual oracle needs to decommit the mask at point x, from the trace.
*/
class CompositionOracleProver {
 public:
  CompositionOracleProver(
      MaybeOwnedPtr<const EvaluationDomain> evaluation_domain,
      MaybeOwnedPtr<CommittedTraceProverBase<BaseFieldElement>> trace,
      MaybeOwnedPtr<CommittedTraceProverBase<ExtensionFieldElement>> composition_trace,
      gsl::span<const std::pair<int64_t, uint64_t>> mask, MaybeOwnedPtr<const Air> air,
      MaybeOwnedPtr<const CompositionPolynomial> composition_polynomial, ProverChannel* channel);

  /*
    Evaluates the composition polynomial over n_cosets cosets.
    The evaluation is done in task_size tasks. This is forwarded to the composition polynomial
    EvalOnCosetBitReversedOutput, see more info there.
  */
  std::vector<ExtensionFieldElement> EvalComposition(uint64_t task_size, uint64_t n_cosets) const;

  /*
    Given queries for the virtual oracle, decommits the correct values from the trace to prove the
    virtual oracle values at these queries.
  */
  void DecommitQueries(const std::vector<std::pair<uint64_t, uint64_t>>& queries) const;

  /*
    Computes the mask of the trace columns at a point.
  */
  void EvalMaskAtPoint(
      const ExtensionFieldElement& point, gsl::span<ExtensionFieldElement> output) const;

  /*
    Composition polynomial degree divided by trace length.
  */
  uint64_t ConstraintsDegreeBound() const;

  const EvaluationDomain& GetEvaluationDomain() const { return *evaluation_domain_; }

  gsl::span<const std::pair<int64_t, uint64_t>> GetMask() const { return mask_; }

  /*
    Total number of trace columns, including the composition_trace columns.
  */
  size_t Width() const;

  MaybeOwnedPtr<CommittedTraceProverBase<BaseFieldElement>> MoveTrace() && {
    ASSERT_RELEASE(
        !composition_trace_.HasValue(),
        "MoveTrace() cannot be called when composition trace is set.");
    return std::move(trace_);
  }

 private:
  std::vector<std::pair<int64_t, uint64_t>> mask_;
  MaybeOwnedPtr<CommittedTraceProverBase<BaseFieldElement>> trace_;
  // composition_trace_ may be null.
  MaybeOwnedPtr<CommittedTraceProverBase<ExtensionFieldElement>> composition_trace_;
  MaybeOwnedPtr<const EvaluationDomain> evaluation_domain_;
  // We give ownership of the AIR to composition polynomial to avoid the following issue:
  // Although CompositionPolynomial has MaybeOwnedPtr<Air>, there is no way to give
  // it ownership. The reason is that CompositionPolynomial is constructed from within the AIR,
  // which cannot give ownership of itself. AIR has no ownership of CompositionPolynomial either.
  MaybeOwnedPtr<const Air> air_;
  MaybeOwnedPtr<const CompositionPolynomial> composition_polynomial_;
  ProverChannel* const channel_;
  std::vector<size_t> trace_widths_;
  std::vector<std::vector<std::pair<int64_t, uint64_t>>> splitted_masks_;
};

class CompositionOracleVerifier {
 public:
  CompositionOracleVerifier(
      MaybeOwnedPtr<const EvaluationDomain> evaluation_domain,
      MaybeOwnedPtr<const CommittedTraceVerifierBase<BaseFieldElement>> trace,
      MaybeOwnedPtr<const CommittedTraceVerifierBase<ExtensionFieldElement>> composition_trace,
      gsl::span<const std::pair<int64_t, uint64_t>> mask, MaybeOwnedPtr<const Air> air,
      MaybeOwnedPtr<const CompositionPolynomial> composition_polynomial, VerifierChannel* channel);

  std::vector<ExtensionFieldElement> VerifyDecommitment(
      std::vector<std::pair<uint64_t, uint64_t>> queries) const;

  /*
    Composition polynomial degree divided by trace length.
  */
  uint64_t ConstraintsDegreeBound() const;

  gsl::span<const std::pair<int64_t, uint64_t>> GetMask() const { return mask_; }

  const CompositionPolynomial& GetCompositionPolynomial() const { return *composition_polynomial_; }

  /*
    Total number of trace columns, including the composition_trace columns.
  */
  size_t Width() const;

  MaybeOwnedPtr<const CommittedTraceVerifierBase<BaseFieldElement>> MoveTrace() && {
    ASSERT_RELEASE(
        !composition_trace_.HasValue(),
        "MoveTrace() cannot be called when composition trace is set.");
    return std::move(trace_);
  }

 private:
  MaybeOwnedPtr<const CommittedTraceVerifierBase<BaseFieldElement>> trace_;
  // composition_trace_ may be null.
  MaybeOwnedPtr<const CommittedTraceVerifierBase<ExtensionFieldElement>> composition_trace_;
  std::vector<std::pair<int64_t, uint64_t>> mask_;
  MaybeOwnedPtr<const EvaluationDomain> evaluation_domain_;
  // We give ownership of the AIR to composition polynomial to avoid the following issue:
  // Although CompositionPolynomial has MaybeOwnedPtr<Air>, there is no way to give
  // it ownership. The reason is that CompositionPolynomial is constructed from within the AIR,
  // which cannot give ownership of itself. AIR has no ownership of CompositionPolynomial either.
  MaybeOwnedPtr<const Air> air_;
  MaybeOwnedPtr<const CompositionPolynomial> composition_polynomial_;
  VerifierChannel* const channel_;
  std::vector<size_t> trace_widths_;
  std::vector<std::vector<std::pair<int64_t, uint64_t>>> splitted_masks_;
};

}  // namespace starkware

#endif  // STARKWARE_STARK_COMPOSITION_ORACLE_H_
