#ifndef STARKWARE_STARK_COMMITTED_TRACE_H_
#define STARKWARE_STARK_COMMITTED_TRACE_H_

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/trace.h"
#include "starkware/algebra/domains/coset.h"
#include "starkware/algebra/domains/evaluation_domain.h"
#include "starkware/algebra/lde/cached_lde_manager.h"
#include "starkware/commitment_scheme/table_prover.h"
#include "starkware/commitment_scheme/table_verifier.h"

namespace starkware {

/*
  Given a trace (a vector of column evaluations over the trace domain), this class is responsible
  for computing the trace LDE over the evaluation domain, and for the commitment and decommitment of
  that LDE.
*/
template <typename FieldElementT>
class CommittedTraceProverBase {
 public:
  virtual ~CommittedTraceProverBase() = default;

  /*
    Returns the number of columns.
  */
  virtual size_t NumColumns() const = 0;

  /*
    Returns the LDE manager.
  */
  virtual CachedLdeManager<FieldElementT>* GetLde() = 0;

  /*
    Computes the trace LDE over the evaluation domain and then commits to the LDE evaluations.

    If eval_in_natural_order is true/false, then each trace column consists of evaluations over a
    natural/bit-reversed order enumeration of the trace_domain, respectively. (see
    multiplicative_group_ordering.h)

    Commitments are expected to be over bit-reversed order evaluations.
    If eval_in_natural_order is true, then the LDE evaluations are bit-reversed prior to commitment.
  */
  virtual void Commit(
      TraceBase<FieldElementT>&& trace, const Coset& trace_domain, bool eval_in_natural_order) = 0;

  /*
    Given queries for the commitment, computes the relevant commitment leaves from the LDE, and
    decommits them. queries is a list of tuples of the form (coset_index, offset, column_index).
  */
  virtual void DecommitQueries(
      gsl::span<const std::tuple<uint64_t, uint64_t, size_t>> queries) const = 0;

  /*
    Computes the mask of the trace columns at a point.
    Its purpose is for out of domain sampling.
    Note that point is of type ExtensionFieldElement regardless of FieldElementT since this function
    is used for out of domain sampling.
  */
  virtual void EvalMaskAtPoint(
      gsl::span<const std::pair<int64_t, uint64_t>> mask, const ExtensionFieldElement& point,
      gsl::span<ExtensionFieldElement> output) const = 0;

  /*
    Calls the LDE manager FinalizeEvaluations function. Once called, no new uncached evaluations
    such as GetLde()->EvalAtPointsNotCached(), will occur anymore.
  */
  virtual void FinalizeEval() = 0;
};

template <typename FieldElementT>
class CommittedTraceProver : public CommittedTraceProverBase<FieldElementT> {
 public:
  /*
    Commitment is done over the evaluation_domain, where each commitment row is of size n_columns.
    Since commitment is expected to be over bit-reversed order evaluations, coset order is
    bit-reversed in CommittedTraceProver (this happens in Commit(), during the creation of the LDE
    manager).

    table_prover_factory is a function that given the size of the data to commit on, creates a
    TableProver which is used for committing and decommitting the data.
  */
  CommittedTraceProver(
      MaybeOwnedPtr<const EvaluationDomain> evaluation_domain, size_t n_columns,
      const TableProverFactory<FieldElementT>& table_prover_factory);

  size_t NumColumns() const override { return n_columns_; }

  CachedLdeManager<FieldElementT>* GetLde() override { return lde_.get(); }

  void Commit(
      TraceBase<FieldElementT>&& trace, const Coset& trace_domain,
      bool eval_in_natural_order) override;

  void DecommitQueries(
      gsl::span<const std::tuple<uint64_t, uint64_t, size_t>> queries) const override;

  void EvalMaskAtPoint(
      gsl::span<const std::pair<int64_t, uint64_t>> mask, const ExtensionFieldElement& point,
      gsl::span<ExtensionFieldElement> output) const override;

  void FinalizeEval() override { lde_->FinalizeEvaluations(); }

 private:
  /*
    Given commitment row indices, computes the rows using lde_. output must be of size NumColumns(),
    and each span inside will be filled with the column evaluations at the given rows.
  */
  void AnswerQueries(
      const std::vector<uint64_t>& rows_to_fetch,
      gsl::span<const gsl::span<FieldElementT>> output) const;

  std::unique_ptr<CachedLdeManager<FieldElementT>> lde_;
  MaybeOwnedPtr<const EvaluationDomain> evaluation_domain_;
  size_t n_columns_;
  std::unique_ptr<TableProver<FieldElementT>> table_prover_;
};

/*
  The verifier equivalent of CommittedTraceProverBase.
*/
template <typename FieldElementT>
class CommittedTraceVerifierBase {
 public:
  virtual ~CommittedTraceVerifierBase() = default;

  virtual size_t NumColumns() const = 0;

  /*
    Reads the commitment.
  */
  virtual void ReadCommitment() = 0;

  /*
    Verifies that the answers to the given queries are indeed the ones committed to by the prover.
    Returns the queries results.
  */
  virtual std::vector<FieldElementT> VerifyDecommitment(
      gsl::span<const std::tuple<uint64_t, uint64_t, size_t>> queries) const = 0;
};

template <typename FieldElementT>
class CommittedTraceVerifier : public CommittedTraceVerifierBase<FieldElementT> {
 public:
  /*
    Given the size of the committed data, table_verifier_factory is a function that creates a
    TableVerifier which is used for reading and verifying commitments.
  */
  CommittedTraceVerifier(
      MaybeOwnedPtr<const EvaluationDomain> evaluation_domain, size_t n_columns,
      const TableVerifierFactory<FieldElementT>& table_verifier_factory);

  size_t NumColumns() const override { return n_columns_; }

  void ReadCommitment() override;

  std::vector<FieldElementT> VerifyDecommitment(
      gsl::span<const std::tuple<uint64_t, uint64_t, size_t>> queries) const override;

 private:
  MaybeOwnedPtr<const EvaluationDomain> evaluation_domain_;
  const size_t n_columns_;
  std::unique_ptr<TableVerifier<FieldElementT>> table_verifier_;
};

}  // namespace starkware

#include "starkware/stark/committed_trace.inl"

#endif  // STARKWARE_STARK_COMMITTED_TRACE_H_
