#include "starkware/stark/composition_oracle.h"

#include <memory>

#include "starkware/channel/annotation_scope.h"
#include "starkware/utils/profiling.h"

namespace starkware {

namespace {

/*
  Given a column index into the aggregated trace, finds the trace index and the column
  index within that trace that the original column corresponds to.
  The trace index is 0 for the original trace and 1 for the composition_trace.
  See SplitMask() for an example.
*/
std::pair<size_t, size_t> ColumnToTraceColumn(size_t column, const std::vector<uint64_t>& widths) {
  for (size_t trace_i = 0; trace_i < widths.size(); ++trace_i) {
    if (column < widths[trace_i]) {
      return {trace_i, column};
    }
    column -= widths[trace_i];
  }
  THROW_STARKWARE_EXCEPTION("Column index out of range.");
}

/*
  Given a mask over the aggregated traces, splits to n_traces different masks for each trace where
  n_traces is 2 if composition_trace has value and 1 otherwise. For example, if trace has 2 columns
  and composition_trace has 4 columns, and the mask is:
    {(10,0), (20,4), (30,1), (40,3), (50,3), (60,5)}
  The split masks will be:
    {(10,0), (30,1)}
    {(20,2), (40,1), (50,1), (60,3)}.
*/
std::vector<std::vector<std::pair<int64_t, uint64_t>>> SplitMask(
    gsl::span<const std::pair<int64_t, uint64_t>> mask, const std::vector<uint64_t>& widths) {
  std::vector<std::vector<std::pair<int64_t, uint64_t>>> masks(widths.size());

  for (const auto& [row, col] : mask) {
    const auto& [trace_i, new_col] = ColumnToTraceColumn(col, widths);
    masks[trace_i].emplace_back(row, new_col);
  }

  return masks;
}

/*
  Given queries in evaluation domain, and a mask over a specific trace (set of columns), finds the
  needed queries to that trace that cover the mask at all original queries.
*/
std::vector<std::tuple<uint64_t, uint64_t, size_t>> QueriesToTraceQueries(
    gsl::span<const std::pair<uint64_t, uint64_t>> queries,
    gsl::span<const std::pair<int64_t, uint64_t>> trace_mask, const size_t trace_length) {
  const size_t n_trace_bits = SafeLog2(trace_length);

  std::vector<std::tuple<uint64_t, uint64_t, size_t>> trace_queries;
  trace_queries.reserve(trace_mask.size() * queries.size());
  for (const auto& [coset_index, offset] : queries) {
    for (const auto& [mask_row, mask_col] : trace_mask) {
      // Add mask item to query.
      const uint64_t new_offset = BitReverse(
          (BitReverse(offset, n_trace_bits) + mask_row) & (trace_length - 1), n_trace_bits);
      trace_queries.emplace_back(coset_index, new_offset, mask_col);
    }
  }

  return trace_queries;
}

}  // namespace

CompositionOracleProver::CompositionOracleProver(
    MaybeOwnedPtr<const EvaluationDomain> evaluation_domain,
    MaybeOwnedPtr<CommittedTraceProverBase<BaseFieldElement>> trace,
    MaybeOwnedPtr<CommittedTraceProverBase<ExtensionFieldElement>> composition_trace,
    gsl::span<const std::pair<int64_t, uint64_t>> mask, MaybeOwnedPtr<const Air> air,
    MaybeOwnedPtr<const CompositionPolynomial> composition_polynomial, ProverChannel* channel)
    : mask_(mask.begin(), mask.end()),
      trace_(std::move(trace)),
      composition_trace_(std::move(composition_trace)),
      evaluation_domain_(std::move(evaluation_domain)),
      air_(std::move(air)),
      composition_polynomial_(std::move(composition_polynomial)),
      channel_(channel) {
  ASSERT_RELEASE(trace_.HasValue(), "trace must not be null.");
  trace_widths_.push_back(trace_->NumColumns());
  if (composition_trace_.HasValue()) {
    trace_widths_.push_back(composition_trace_->NumColumns());
  }

  splitted_masks_ = SplitMask(mask, trace_widths_);
}

std::vector<ExtensionFieldElement> CompositionOracleProver::EvalComposition(
    uint64_t task_size, uint64_t n_cosets) const {
  const uint64_t trace_length = evaluation_domain_->TraceSize();
  const size_t n_segments = n_cosets;
  ASSERT_RELEASE(
      n_segments <= evaluation_domain_->NumCosets(),
      "Composition polynomial degree bound is larger than evaluation domain.");
  auto evaluation = ExtensionFieldElement::UninitializedVector(n_cosets * trace_length);

  // Allocate storage for bit-reversal.
  std::vector<std::vector<BaseFieldElement>> tmp_storages;
  if (!trace_->GetLde()->IsEvalNaturallyOrdered()) {
    tmp_storages.reserve(trace_widths_[0]);
    for (size_t i = 0; i < trace_widths_[0]; ++i) {
      tmp_storages.emplace_back(BaseFieldElement::UninitializedVector(trace_length));
    }
  }
  std::vector<std::vector<ExtensionFieldElement>> composition_tmp_storages;
  if (composition_trace_.HasValue() && !composition_trace_->GetLde()->IsEvalNaturallyOrdered()) {
    composition_tmp_storages.reserve(trace_widths_[1]);
    for (size_t i = 0; i < trace_widths_[1]; ++i) {
      composition_tmp_storages.emplace_back(
          ExtensionFieldElement::UninitializedVector(trace_length));
    }
  }

  const size_t log_n_cosets = SafeLog2(evaluation_domain_->NumCosets());
  for (uint64_t coset_index = 0; coset_index < n_segments; coset_index++) {
    std::vector<gsl::span<const BaseFieldElement>> trace_evals;
    std::vector<gsl::span<const ExtensionFieldElement>> composition_trace_evals;

    // Evaluate trace at the coset.
    auto eval_trace = [&](const auto& trace, auto* eval_vec, auto* tmp_storages) {
      using FieldElementT = std::decay_t<decltype((*eval_vec)[0][0])>;
      ProfilingBlock profiling_lde_block("LDE2");
      const std::vector<std::vector<FieldElementT>>* coset_columns_eval =
          trace->GetLde()->EvalOnCoset(coset_index);
      profiling_lde_block.CloseBlock();

      ProfilingBlock profiling_block("BitReversal of columns");
      eval_vec->reserve(coset_columns_eval->size());
      size_t tmp_storage_index = 0;
      for (auto& coset_column_eval : *coset_columns_eval) {
        if (trace->GetLde()->IsEvalNaturallyOrdered()) {
          eval_vec->push_back(coset_column_eval);
        } else {
          ASSERT_RELEASE(
              tmp_storage_index < tmp_storages->size(), "Not enough temporary storages.");
          BitReverseVector(
              gsl::make_span(coset_column_eval),
              gsl::make_span((*tmp_storages)[tmp_storage_index]));
          eval_vec->push_back(gsl::make_span((*tmp_storages)[tmp_storage_index]));
          tmp_storage_index++;
        }
      }
    };
    eval_trace(trace_, &trace_evals, &tmp_storages);
    if (composition_trace_.HasValue()) {
      eval_trace(composition_trace_, &composition_trace_evals, &composition_tmp_storages);
    }

    const size_t coset_natural_index = BitReverse(coset_index, log_n_cosets);
    const BaseFieldElement coset_offset = evaluation_domain_->CosetOffsets()[coset_natural_index];
    ProfilingBlock composition_block("Actual point-wise computation");
    composition_polynomial_->EvalOnCosetBitReversedOutput(
        coset_offset, trace_evals, composition_trace_evals,
        gsl::make_span(evaluation).subspan(coset_index * trace_length, trace_length), task_size);
  }
  return evaluation;
}

void CompositionOracleProver::DecommitQueries(
    const std::vector<std::pair<uint64_t, uint64_t>>& queries) const {
  {
    AnnotationScope scope(channel_, "Trace");
    const auto trace_queries =
        QueriesToTraceQueries(queries, splitted_masks_[0], evaluation_domain_->TraceSize());

    trace_->DecommitQueries(trace_queries);
  }

  if (composition_trace_.HasValue()) {
    AnnotationScope scope(channel_, "Composition Trace");
    const auto trace_queries =
        QueriesToTraceQueries(queries, splitted_masks_[1], evaluation_domain_->TraceSize());

    composition_trace_->DecommitQueries(trace_queries);
  }
}

void CompositionOracleProver::EvalMaskAtPoint(
    const ExtensionFieldElement& point, gsl::span<ExtensionFieldElement> output) const {
  ASSERT_RELEASE(output.size() == mask_.size(), "Wrong output size.");
  std::vector<std::vector<ExtensionFieldElement>> trace_mask_evaluations;
  trace_mask_evaluations.reserve(trace_widths_.size());
  {
    auto eval = ExtensionFieldElement::UninitializedVector(splitted_masks_[0].size());
    trace_->EvalMaskAtPoint(splitted_masks_[0], point, eval);
    trace_mask_evaluations.push_back(std::move(eval));
  }

  if (composition_trace_.HasValue()) {
    auto eval = ExtensionFieldElement::UninitializedVector(splitted_masks_[1].size());
    composition_trace_->EvalMaskAtPoint(splitted_masks_[1], point, eval);
    trace_mask_evaluations.push_back(std::move(eval));
  }

  std::vector<size_t> mask_offset_in_trace(trace_widths_.size());
  for (size_t mask_i = 0; mask_i < mask_.size(); ++mask_i) {
    const auto& mask_col = mask_[mask_i].second;
    const auto& trace_i = ColumnToTraceColumn(mask_col, trace_widths_).first;
    output[mask_i] = trace_mask_evaluations[trace_i][mask_offset_in_trace[trace_i]++];
  }
}

uint64_t CompositionOracleProver::ConstraintsDegreeBound() const {
  const uint64_t trace_length = evaluation_domain_->TraceSize();
  return SafeDiv(composition_polynomial_->GetDegreeBound(), trace_length);
}

size_t CompositionOracleProver::Width() const { return Sum(trace_widths_); }

CompositionOracleVerifier::CompositionOracleVerifier(
    MaybeOwnedPtr<const EvaluationDomain> evaluation_domain,
    MaybeOwnedPtr<const CommittedTraceVerifierBase<BaseFieldElement>> trace,
    MaybeOwnedPtr<const CommittedTraceVerifierBase<ExtensionFieldElement>> composition_trace,
    gsl::span<const std::pair<int64_t, uint64_t>> mask, MaybeOwnedPtr<const Air> air,
    MaybeOwnedPtr<const CompositionPolynomial> composition_polynomial, VerifierChannel* channel)
    : trace_(std::move(trace)),
      composition_trace_(std::move(composition_trace)),
      mask_(mask.begin(), mask.end()),
      evaluation_domain_(std::move(evaluation_domain)),
      air_(std::move(air)),
      composition_polynomial_(std::move(composition_polynomial)),
      channel_(channel) {
  trace_widths_.push_back(trace_->NumColumns());
  if (composition_trace_.HasValue()) {
    trace_widths_.push_back(composition_trace_->NumColumns());
  }

  splitted_masks_ = SplitMask(mask, trace_widths_);
}

std::vector<ExtensionFieldElement> CompositionOracleVerifier::VerifyDecommitment(
    std::vector<std::pair<uint64_t, uint64_t>> queries) const {
  std::vector<BaseFieldElement> trace_mask_values;
  {
    AnnotationScope scope(channel_, "Trace");
    const auto trace_queries =
        QueriesToTraceQueries(queries, splitted_masks_[0], evaluation_domain_->TraceSize());

    trace_mask_values = trace_->VerifyDecommitment(trace_queries);
  }

  std::vector<ExtensionFieldElement> composition_trace_mask_values;
  if (composition_trace_.HasValue()) {
    AnnotationScope scope(channel_, "Composition Trace");
    const auto trace_queries =
        QueriesToTraceQueries(queries, splitted_masks_[1], evaluation_domain_->TraceSize());

    composition_trace_mask_values = composition_trace_->VerifyDecommitment(trace_queries);
  }

  // Compute composition polynomial at queries.
  const size_t log_n_cosets = SafeLog2(evaluation_domain_->NumCosets());
  std::vector<ExtensionFieldElement> oracle_evaluations;
  oracle_evaluations.reserve(queries.size());
  size_t i = 0;
  for (const auto& [coset_index, offset] : queries) {
    // Fetch neighbors from decommitments.
    auto neighbors = gsl::make_span(trace_mask_values)
                         .subspan(splitted_masks_[0].size() * i, splitted_masks_[0].size());
    auto composition_neighbors =
        composition_trace_.HasValue()
            ? gsl::make_span(composition_trace_mask_values)
                  .subspan(splitted_masks_[1].size() * i, splitted_masks_[1].size())
            : gsl::make_span(std::vector<ExtensionFieldElement>{});
    i++;

    // Evaluate composition polynomial at point, given neighbors.
    const size_t coset_natural_index = BitReverse(coset_index, log_n_cosets);
    const BaseFieldElement point = evaluation_domain_->ElementByIndex(coset_natural_index, offset);
    oracle_evaluations.push_back(
        composition_polynomial_->EvalAtPoint(point, neighbors, composition_neighbors));
  }

  return oracle_evaluations;
}

uint64_t CompositionOracleVerifier::ConstraintsDegreeBound() const {
  const uint64_t trace_length = evaluation_domain_->TraceSize();
  return SafeDiv(composition_polynomial_->GetDegreeBound(), trace_length);
}

size_t CompositionOracleVerifier::Width() const { return Sum(trace_widths_); }

}  // namespace starkware
