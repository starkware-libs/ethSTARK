#include "starkware/composition_polynomial/breaker.h"

#include "starkware/algebra/fft/fft.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/utils/task_manager.h"

namespace starkware {

PolynomialBreak::PolynomialBreak(const Coset& coset, size_t log_breaks)
    : coset_(coset), log_breaks_(log_breaks) {
  ASSERT_RELEASE(
      log_breaks <= SafeLog2(coset_.Size()),
      "Number of breaks cannot be larger than the coset size.");
}

std::vector<gsl::span<const ExtensionFieldElement>> PolynomialBreak::Break(
    const gsl::span<const ExtensionFieldElement>& evaluation,
    gsl::span<ExtensionFieldElement> output) const {
  ASSERT_RELEASE(evaluation.size() == coset_.Size(), "Wrong size of evaluation.");
  ASSERT_RELEASE(output.size() == coset_.Size(), "Wrong size of output.");

  // Apply log_breaks_ layers of IFFT to get the evaluations of the h_i's.
  gsl::span<const ExtensionFieldElement> src = evaluation;
  auto dst = ExtensionFieldElement::UninitializedVector(evaluation.size());
  gsl::span<ExtensionFieldElement> dst_span = gsl::make_span(dst);
  IfftReverseToNatural(src, dst_span, coset_.Generator(), coset_.Offset(), log_breaks_);

  // Evaluations of h_i are interleaved. Normalize IFFT output and reorder evaluations such that
  // output contains consecutive evaluations of the h_i.
  const size_t n_breaks = Pow2(log_breaks_);
  const size_t chunk_size = evaluation.size() >> log_breaks_;
  ExtensionFieldElement correction_factor = ExtensionFieldElement::FromUint(n_breaks).Inverse();
  TaskManager::GetInstance().ParallelFor(
      chunk_size,
      [n_breaks, chunk_size, &dst, output, &correction_factor](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          for (size_t break_idx = 0; break_idx < n_breaks; ++break_idx) {
            output[break_idx * chunk_size + i] = dst[i * n_breaks + break_idx] * correction_factor;
          }
        }
      },
      chunk_size);

  std::vector<gsl::span<const ExtensionFieldElement>> results;
  results.reserve(n_breaks);
  for (size_t i = 0; i < n_breaks; ++i) {
    results.emplace_back(output.subspan(i * chunk_size, chunk_size));
  }

  return results;
}

ExtensionFieldElement PolynomialBreak::EvalFromSamples(
    gsl::span<const ExtensionFieldElement> samples, const ExtensionFieldElement& point) const {
  ASSERT_RELEASE(samples.size() == Pow2(log_breaks_), "Wrong size of samples.");
  return HornerEval(point, samples);
}

}  // namespace starkware
