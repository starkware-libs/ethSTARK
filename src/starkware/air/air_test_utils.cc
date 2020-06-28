#include "starkware/air/air_test_utils.h"

#include "starkware/algebra/domains/evaluation_domain.h"
#include "starkware/algebra/lde/lde_manager.h"
#include "starkware/math/math.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {

int64_t ComputeCompositionDegree(
    const Air& air, const Trace& trace, gsl::span<const ExtensionFieldElement> random_coefficients,
    const size_t num_of_cosets) {
  ASSERT_RELEASE((trace.Width() > 0) && (trace.Length() > 0), "Trace must not be empty.");

  // Evaluation domain specifications.
  const size_t coset_size = trace.Length();
  const size_t evaluation_domain_size =
      Pow2(Log2Ceil(air.GetCompositionPolynomialDegreeBound() * num_of_cosets));
  const size_t n_cosets = SafeDiv(evaluation_domain_size, coset_size);
  EvaluationDomain domain(coset_size, n_cosets);
  auto cosets = domain.CosetOffsets();
  const Coset source_domain_coset(coset_size, BaseFieldElement::One());

  // Allocate storage for trace LDE evaluations.
  std::unique_ptr<LdeManager<BaseFieldElement>> lde_manager =
      MakeLdeManager<BaseFieldElement>(source_domain_coset);
  std::vector<std::vector<BaseFieldElement>> trace_lde;
  trace_lde.reserve(trace.Width());
  for (size_t i = 0; i < trace.Width(); ++i) {
    lde_manager->AddEvaluation(trace.GetColumn(i));
    trace_lde.push_back(BaseFieldElement::UninitializedVector(coset_size));
  }

  // Construct composition polynomial.
  std::unique_ptr<CompositionPolynomial> composition_poly =
      air.CreateCompositionPolynomial(domain.TraceGenerator(), random_coefficients);

  // Evaluate composition.
  std::vector<ExtensionFieldElement> evaluation =
      ExtensionFieldElement::UninitializedVector(evaluation_domain_size);
  for (size_t i = 0; i < n_cosets; ++i) {
    const BaseFieldElement& coset_offset = cosets[BitReverse(i, SafeLog2(n_cosets))];
    lde_manager->EvalOnCoset(
        coset_offset, std::vector<gsl::span<BaseFieldElement>>(trace_lde.begin(), trace_lde.end()));

    constexpr uint64_t kTaskSize = 256;

    composition_poly->EvalOnCosetBitReversedOutput(
        coset_offset,
        std::vector<gsl::span<const BaseFieldElement>>(trace_lde.begin(), trace_lde.end()), {},
        gsl::make_span(evaluation).subspan(i * coset_size, coset_size), kTaskSize);
  }

  // Compute degree.
  const auto coset = Coset(evaluation_domain_size, BaseFieldElement::One());
  const auto lde = MakeLdeManager<ExtensionFieldElement>(coset, /*eval_in_natural_order=*/false);
  lde->AddEvaluation(std::move(evaluation));
  return lde->GetEvaluationDegree(0);
}

}  // namespace starkware
