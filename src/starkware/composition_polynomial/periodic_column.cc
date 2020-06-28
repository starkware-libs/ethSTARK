#include "starkware/composition_polynomial/periodic_column.h"

namespace starkware {

PeriodicColumn::PeriodicColumn(gsl::span<const BaseFieldElement> values, uint64_t trace_size)
    : period_in_trace_(values.size()),
      n_copies_(SafeDiv(trace_size, period_in_trace_)),
      lde_manager_(
          Coset(values.size(), BaseFieldElement::One()),
          /*eval_in_natural_order=*/true) {
  lde_manager_.AddEvaluation(values);
}

auto PeriodicColumn::GetCoset(const BaseFieldElement& start_point, const size_t coset_size) const
    -> CosetEvaluation {
  const BaseFieldElement offset = Pow(start_point, n_copies_);
  ASSERT_RELEASE(
      coset_size == n_copies_ * period_in_trace_,
      "coset_size must be the same as the size of the coset that was used to create the "
      "PeriodicColumn.");

  // Allocate storage for the LDE computation.
  std::vector<BaseFieldElement> period_on_coset =
      BaseFieldElement::UninitializedVector(period_in_trace_);
  std::array<gsl::span<BaseFieldElement>, 1> output_spans{gsl::make_span(period_on_coset)};
  lde_manager_.EvalOnCoset(offset, output_spans);
  return CosetEvaluation(std::move(period_on_coset));
}

}  // namespace starkware
