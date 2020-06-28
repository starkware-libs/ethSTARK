#include "starkware/stark/oods.h"

#include "starkware/air/boundary/boundary_air.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/composition_polynomial/breaker.h"

namespace starkware {
namespace oods {

std::pair<CompositionTrace, Coset> BreakCompositionPolynomial(
    gsl::span<const ExtensionFieldElement> composition_evaluation, size_t n_breaks,
    const Coset& domain) {
  const size_t log_n_breaks = SafeLog2(n_breaks);
  const PolynomialBreak poly_break = PolynomialBreak(domain, log_n_breaks);
  auto output = ExtensionFieldElement::UninitializedVector(composition_evaluation.size());
  const std::vector<gsl::span<const ExtensionFieldElement>> output_spans =
      poly_break.Break(composition_evaluation, output);
  const size_t trace_length = SafeDiv(domain.Size(), n_breaks);
  const Coset composition_trace_domain(trace_length, Pow(BaseFieldElement::Generator(), n_breaks));
  return {CompositionTrace::CopyFrom(output_spans), composition_trace_domain};
}

std::unique_ptr<Air> CreateBoundaryAir(
    uint64_t trace_length, size_t n_columns,
    std::vector<std::tuple<size_t, ExtensionFieldElement, ExtensionFieldElement>>&&
        boundary_constraints) {
  return std::make_unique<BoundaryAir>(trace_length, n_columns, std::move(boundary_constraints));
}

std::vector<std::tuple<size_t, ExtensionFieldElement, ExtensionFieldElement>> ProveOods(
    ProverChannel* channel, const CompositionOracleProver& original_oracle,
    const CommittedTraceProverBase<ExtensionFieldElement>& composition_trace) {
  AnnotationScope scope(channel, "OODS values");
  std::vector<std::tuple<size_t, ExtensionFieldElement, ExtensionFieldElement>>
      boundary_constraints;
  const ExtensionFieldElement point =
      channel->GetRandomFieldElementFromVerifier("Evaluation point");
  const ExtensionFieldElement conj_point = point.GetFrobenius();
  const gsl::span<const std::pair<int64_t, uint64_t>>& trace_mask = original_oracle.GetMask();
  ProfilingBlock profiling_block("Eval at OODS point");

  // Send the mask values of the original trace to the channel and generate its boundary
  // constraints.
  {
    // Evaluate the mask at point.
    auto trace_evaluation_at_mask = ExtensionFieldElement::UninitializedVector(trace_mask.size());
    original_oracle.EvalMaskAtPoint(point, trace_evaluation_at_mask);
    std::vector<bool> cols_seen(original_oracle.Width(), false);
    const BaseFieldElement& trace_gen = original_oracle.GetEvaluationDomain().TraceGenerator();
    // Send the mask values and create the LHS of the boundary constraints.
    for (size_t i = 0; i < trace_evaluation_at_mask.size(); ++i) {
      const auto trace_eval_at_idx = trace_evaluation_at_mask.at(i);
      channel->SendFieldElement(trace_eval_at_idx, std::to_string(i));
      const auto& [row_offset, column_index] = trace_mask[i];
      const auto row_element = Pow(trace_gen, row_offset);
      boundary_constraints.emplace_back(column_index, point * row_element, trace_eval_at_idx);
      // Add the corresponding boundary constraint on the conjugate element to guarantee that the
      // trace is defined over the base field. This is done only once for each column.
      if (!cols_seen[column_index]) {
        cols_seen[column_index] = true;
        const auto conj_trace_eval_at_idx_value = trace_eval_at_idx.GetFrobenius();
        boundary_constraints.emplace_back(
            column_index, conj_point * row_element, conj_trace_eval_at_idx_value);
      }
    }
  }
  // Send the mask values of the broken composition trace to the channel and generate its boundary
  // constraints.
  {
    // Compute a simple mask consisting of one row for broken side.
    const size_t n_breaks = composition_trace.NumColumns();
    const size_t trace_mask_size = trace_mask.size();
    std::vector<std::pair<int64_t, uint64_t>> broken_eval_mask;
    broken_eval_mask.reserve(n_breaks);
    for (size_t column_index = 0; column_index < n_breaks; ++column_index) {
      broken_eval_mask.emplace_back(0, column_index);
    }
    // Evaluate the mask at point.
    const ExtensionFieldElement point_transformed = Pow(point, n_breaks);
    auto broken_evaluation = ExtensionFieldElement::UninitializedVector(n_breaks);
    composition_trace.EvalMaskAtPoint(broken_eval_mask, point_transformed, broken_evaluation);
    // Send the mask values and create the RHS of the boundary constraints.
    for (size_t i = 0; i < broken_evaluation.size(); ++i) {
      channel->SendFieldElement(broken_evaluation.at(i), std::to_string(trace_mask_size + i));
      // Assuming all broken_column appear right after trace columns.
      boundary_constraints.emplace_back(
          original_oracle.Width() + i, point_transformed, broken_evaluation.at(i));
    }
  }
  return boundary_constraints;
}

std::vector<std::tuple<size_t, ExtensionFieldElement, ExtensionFieldElement>> VerifyOods(
    const EvaluationDomain& evaluation_domain, VerifierChannel* channel,
    const CompositionOracleVerifier& original_oracle, const Coset& composition_eval_domain) {
  AnnotationScope scope(channel, "OODS values");
  const BaseFieldElement& trace_gen = evaluation_domain.TraceGenerator();
  std::vector<std::tuple<size_t, ExtensionFieldElement, ExtensionFieldElement>>
      boundary_constraints;
  const ExtensionFieldElement point =
      channel->GetRandomFieldElementFromVerifier("Evaluation point");
  const ExtensionFieldElement conj_point = point.GetFrobenius();

  // Receive the mask values of the original trace from the channel and generate its boundary
  // constraints.
  const auto& mask = original_oracle.GetMask();
  const size_t trace_mask_size = mask.size();
  std::vector<ExtensionFieldElement> original_oracle_mask_evaluation;
  original_oracle_mask_evaluation.reserve(mask.size());
  std::vector<bool> cols_seen(original_oracle.Width(), false);
  for (size_t i = 0; i < mask.size(); ++i) {
    const auto value = channel->ReceiveFieldElement<ExtensionFieldElement>(std::to_string(i));
    const auto& [row_offset, column_index] = mask[i];
    original_oracle_mask_evaluation.push_back(value);
    boundary_constraints.emplace_back(column_index, point * Pow(trace_gen, row_offset), value);
    // Add the corresponding boundary constarint on the conjugate element to guarantee that the
    // trace is defined over the base field. This is done only once for each column.
    if (!cols_seen[column_index]) {
      cols_seen[column_index] = true;
      const auto conj_trace_eval_at_idx_value = value.GetFrobenius();
      boundary_constraints.emplace_back(
          column_index, conj_point * Pow(trace_gen, row_offset), conj_trace_eval_at_idx_value);
    }
  }
  // Evaluate the value of the composition polynomial at point using the original trace mask
  // evaluation.
  const ExtensionFieldElement trace_side_value =
      original_oracle.GetCompositionPolynomial().EvalAtPoint(
          point, original_oracle_mask_evaluation, {});

  const size_t n_breaks = original_oracle.ConstraintsDegreeBound();
  auto poly_break = PolynomialBreak(composition_eval_domain, SafeLog2(n_breaks));

  // Receive the mask values of the broken composition trace from the channel and generate its
  // boundary constraints.
  const ExtensionFieldElement point_transformed = Pow(point, n_breaks);
  std::vector<ExtensionFieldElement> broken_evaluation;
  broken_evaluation.reserve(n_breaks);
  for (size_t i = 0; i < n_breaks; ++i) {
    const auto value =
        channel->ReceiveFieldElement<ExtensionFieldElement>(std::to_string(trace_mask_size + i));
    broken_evaluation.push_back(value);
    boundary_constraints.emplace_back(
        original_oracle.Width() + i, point_transformed, broken_evaluation.at(i));
  }
  // Evaluate the value of the composition polynomial at point using the broken trace mask
  // evaluation.
  const ExtensionFieldElement broken_side_value =
      poly_break.EvalFromSamples(broken_evaluation, point);

  // Verify the OODS equation is satisfied.
  ASSERT_RELEASE(
      trace_side_value == broken_side_value, "Out of domain sampling verification failed.");

  return boundary_constraints;
}

}  // namespace oods
}  // namespace starkware
