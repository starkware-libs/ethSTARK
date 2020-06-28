#include "starkware/stark/committed_trace.h"

#include <map>
#include <set>

#include "starkware/utils/profiling.h"

namespace starkware {

namespace committed_trace {
namespace details {

/*
  Creates a cached LDE manager with coset offsets in bit-reversed order.
*/
template <typename FieldElementT>
inline std::unique_ptr<CachedLdeManager<FieldElementT>> CreateLdeManager(
    const Coset& trace_domain, const EvaluationDomain& evaluation_domain,
    bool eval_in_natural_order) {
  // Create LDE manager.
  std::unique_ptr<LdeManager<FieldElementT>> lde_manager =
      MakeLdeManager<FieldElementT>(trace_domain, eval_in_natural_order);

  // Bit-reverse coset offsets.
  const size_t n_cosets = evaluation_domain.NumCosets();
  std::vector<BaseFieldElement> coset_offsets;
  coset_offsets.reserve(n_cosets);
  const size_t log_cosets = SafeLog2(n_cosets);
  for (uint64_t i = 0; i < n_cosets; ++i) {
    coset_offsets.emplace_back(evaluation_domain.CosetOffsets()[BitReverse(i, log_cosets)]);
  }

  // Create CachedLdeManager.
  return std::make_unique<CachedLdeManager<FieldElementT>>(
      TakeOwnershipFrom(std::move(lde_manager)), std::move(coset_offsets));
}

}  // namespace details
}  // namespace committed_trace

// ------------------------------------------------------------------------------------------
//  Prover side
// ------------------------------------------------------------------------------------------

template <typename FieldElementT>
CommittedTraceProver<FieldElementT>::CommittedTraceProver(
    MaybeOwnedPtr<const EvaluationDomain> evaluation_domain, size_t n_columns,
    const TableProverFactory<FieldElementT>& table_prover_factory)
    : evaluation_domain_(std::move(evaluation_domain)),
      n_columns_(n_columns),
      table_prover_(table_prover_factory(
          evaluation_domain_->NumCosets(), evaluation_domain_->TraceSize(), n_columns_)) {}

template <typename FieldElementT>
void CommittedTraceProver<FieldElementT>::Commit(
    TraceBase<FieldElementT>&& trace, const Coset& trace_domain, bool eval_in_natural_order) {
  ASSERT_RELEASE(trace.Width() == n_columns_, "Wrong number of columns.");
  ASSERT_RELEASE(trace.Length() == evaluation_domain_->TraceSize(), "Wrong trace length.");

  // Create an LDE manager and add column evaluations.
  lde_ = committed_trace::details::CreateLdeManager<FieldElementT>(
      trace_domain, *evaluation_domain_, eval_in_natural_order);

  ProfilingBlock interpolation_block("Interpolation");
  auto columns = std::move(trace).ConsumeAsColumnsVector();
  for (auto&& column : columns) {
    lde_->AddEvaluation(std::move(column));
  }
  lde_->FinalizeAdding();
  interpolation_block.CloseBlock();

  // On each coset, evaluate the LDE and then add evaluation to the commitment scheme (bit-reverse
  // the evaluations if necessary).
  const size_t trace_length = evaluation_domain_->TraceSize();
  TaskManager::GetInstance().ParallelFor(
      evaluation_domain_->NumCosets(),
      [this, eval_in_natural_order, trace_length](const TaskInfo& task_info) {
        const size_t coset_index = task_info.start_idx;

        // Allocate storage for bit-reversing the evaluations.
        std::vector<std::vector<FieldElementT>> bitrev_evaluations;
        bitrev_evaluations.reserve(n_columns_);

        // Evaluate the LDE on the coset.
        ProfilingBlock lde_block("LDE");
        const auto lde_evaluations = lde_->EvalOnCoset(coset_index);
        std::vector<gsl::span<const FieldElementT>> lde_evaluations_spans;
        lde_evaluations_spans.reserve(lde_evaluations->size());
        for (const auto& lde_evaluation : *lde_evaluations) {
          lde_evaluations_spans.push_back(lde_evaluation);
        }
        lde_block.CloseBlock();

        // Bit-reverse if necessary.
        if (eval_in_natural_order) {
          ProfilingBlock bit_reversal_block("BitReversal of columns");
          for (const auto& lde_evaluation : *lde_evaluations) {
            bitrev_evaluations.emplace_back(FieldElementT::UninitializedVector(trace_length));
            BitReverseVector(
                gsl::make_span(lde_evaluation), gsl::make_span(bitrev_evaluations.back()));
          }
          bit_reversal_block.CloseBlock();
        }

        // Add the LDE coset evaluation to the commitment scheme.
        ProfilingBlock commit_to_lde_block("Commit to LDE");
        if (eval_in_natural_order) {
          table_prover_->AddSegmentForCommitment(
              {bitrev_evaluations.begin(), bitrev_evaluations.end()}, coset_index);
        } else {
          table_prover_->AddSegmentForCommitment(lde_evaluations_spans, coset_index);
        }
        commit_to_lde_block.CloseBlock();
      });

  // Commit to the LDE evaluations.
  table_prover_->Commit();
}

template <typename FieldElementT>
void CommittedTraceProver<FieldElementT>::DecommitQueries(
    gsl::span<const std::tuple<uint64_t, uint64_t, size_t>> queries) const {
  const uint64_t trace_length = evaluation_domain_->TraceSize();

  // The commitment items we need to open.
  std::set<RowCol> data_queries;
  for (const auto& [coset_index, offset, column_index] : queries) {
    ASSERT_RELEASE(coset_index < evaluation_domain_->NumCosets(), "Coset index out of range.");
    ASSERT_RELEASE(offset < trace_length, "Coset offset out of range.");
    ASSERT_RELEASE(column_index < NumColumns(), "Column index out of range.");
    data_queries.emplace(coset_index * trace_length + offset, column_index);
  }

  // Commitment rows to fetch.
  const std::vector<uint64_t> rows_to_fetch =
      table_prover_->StartDecommitmentPhase(data_queries, /*integrity_queries=*/{});

  // Prepare storage for the requested rows.
  std::vector<std::vector<FieldElementT>> elements_data;
  elements_data.reserve(NumColumns());
  for (size_t i = 0; i < NumColumns(); ++i) {
    elements_data.push_back(FieldElementT::UninitializedVector(rows_to_fetch.size()));
  }

  // Computes the rows.
  AnswerQueries(
      rows_to_fetch,
      std::vector<gsl::span<FieldElementT>>(elements_data.begin(), elements_data.end()));

  // Decommit the rows.
  table_prover_->Decommit(
      std::vector<gsl::span<const FieldElementT>>(elements_data.begin(), elements_data.end()));
}

template <typename FieldElementT>
void CommittedTraceProver<FieldElementT>::EvalMaskAtPoint(
    gsl::span<const std::pair<int64_t, uint64_t>> mask, const ExtensionFieldElement& point,
    gsl::span<ExtensionFieldElement> output) const {
  ASSERT_RELEASE(mask.size() == output.size(), "Mask size does not equal output size.");

  const BaseFieldElement& trace_gen = evaluation_domain_->TraceGenerator();

  // A map from column index to pairs (mask_row_offset, mask_index).
  std::map<uint64_t, std::vector<std::pair<int64_t, size_t>>> columns;
  for (size_t mask_index = 0; mask_index < mask.size(); ++mask_index) {
    const auto& [row_offset, column_index] = mask[mask_index];
    ASSERT_RELEASE(row_offset >= 0, "Negative mask row offsets are not supported.");
    columns[column_index].emplace_back(row_offset, mask_index);
  }

  // Evaluate mask at each column.
  for (const auto& [column_index, column_offsets] : columns) {
    // Compute points to evaluate at.
    std::vector<ExtensionFieldElement> points;
    points.reserve(column_offsets.size());
    for (const auto& offset_pair : column_offsets) {
      const int64_t row_offset = offset_pair.first;
      points.push_back(point * Pow(trace_gen, row_offset));
    }

    // Allocate output.
    auto column_output = ExtensionFieldElement::UninitializedVector(column_offsets.size());

    // Evaluate.
    lde_->EvalAtPointsNotCached(column_index, points, column_output);

    // Place outputs at the correct place.
    for (size_t i = 0; i < column_offsets.size(); ++i) {
      const size_t mask_index = column_offsets[i].second;
      output[mask_index] = column_output.at(i);
    }
  }
}

template <typename FieldElementT>
void CommittedTraceProver<FieldElementT>::AnswerQueries(
    const std::vector<uint64_t>& rows_to_fetch,
    gsl::span<const gsl::span<FieldElementT>> output) const {
  const uint64_t trace_length = evaluation_domain_->TraceSize();

  // Translate queries to coset and point indices.
  std::vector<std::pair<uint64_t, uint64_t>> coset_and_point_indices;
  coset_and_point_indices.reserve(rows_to_fetch.size());
  for (const uint64_t row : rows_to_fetch) {
    const uint64_t coset_index = row / trace_length;
    const uint64_t offset = row % trace_length;
    coset_and_point_indices.emplace_back(coset_index, offset);
  }

  // Call CachedLdeManager::EvalAtPoints().
  lde_->EvalAtPoints(coset_and_point_indices, output);
}

// ------------------------------------------------------------------------------------------
//  Verifier side
// ------------------------------------------------------------------------------------------

template <typename FieldElementT>
CommittedTraceVerifier<FieldElementT>::CommittedTraceVerifier(
    MaybeOwnedPtr<const EvaluationDomain> evaluation_domain, size_t n_columns,
    const TableVerifierFactory<FieldElementT>& table_verifier_factory)
    : evaluation_domain_(std::move(evaluation_domain)),
      n_columns_(n_columns),
      table_verifier_(table_verifier_factory(evaluation_domain_->Size(), n_columns)) {}

template <typename FieldElementT>
void CommittedTraceVerifier<FieldElementT>::ReadCommitment() {
  table_verifier_->ReadCommitment();
}

template <typename FieldElementT>
std::vector<FieldElementT> CommittedTraceVerifier<FieldElementT>::VerifyDecommitment(
    gsl::span<const std::tuple<uint64_t, uint64_t, size_t>> queries) const {
  const uint64_t trace_length = evaluation_domain_->TraceSize();

  // The commitment items we need to open.
  std::set<RowCol> data_queries;
  for (const auto& [coset_index, offset, column_index] : queries) {
    ASSERT_RELEASE(coset_index < evaluation_domain_->NumCosets(), "Coset index out of range.");
    ASSERT_RELEASE(offset < trace_length, "Coset offset out of range.");
    ASSERT_RELEASE(column_index < n_columns_, "Column index out of range.");
    data_queries.emplace(coset_index * trace_length + offset, column_index);
  }

  // Query results.
  std::map<RowCol, FieldElementT> data_responses =
      table_verifier_->Query(data_queries, {} /* no integrity queries */);

  ASSERT_RELEASE(
      table_verifier_->VerifyDecommitment(data_responses),
      "Prover responses did not pass integrity check: Proof rejected.");

  // Allocate storage for responses.
  std::vector<FieldElementT> query_responses;
  query_responses.reserve(queries.size());

  // Place query results at the correct place.
  for (const auto& [coset_index, offset, column_index] : queries) {
    query_responses.push_back(
        data_responses.at(RowCol(coset_index * trace_length + offset, column_index)));
  }

  return query_responses;
}

}  // namespace starkware
