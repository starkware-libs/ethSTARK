#include "starkware/air/test_air/test_air.h"

namespace starkware {

/*
  The public periodic values.
*/
const std::vector<BaseFieldElement> TestAir::kPeriodicValues = {BaseFieldElement::FromUint(2),
                                                                BaseFieldElement::FromUint(10)};
/*
  The public constant value.
*/
const BaseFieldElement TestAir::kConst = BaseFieldElement::FromUint(16);

std::vector<std::pair<int64_t, uint64_t>> TestAir::GetMask() const {
  return {{0, 0}, {slackness_factor_, 0}, {0, 1}};
}

std::unique_ptr<CompositionPolynomial> TestAir::CreateCompositionPolynomial(
    const BaseFieldElement& trace_generator,
    gsl::span<const ExtensionFieldElement> random_coefficients) const {
  Builder builder(/*num_periodic_columns=*/1);
  const uint64_t composition_degree_bound = GetCompositionPolynomialDegreeBound();

  // Prepare a list of all the values used in expressions of the form 'point^value', where point
  // represents the field elements that will be substituted in the composition polynomial.
  const std::vector<uint64_t> point_exponents = {
      original_trace_length_,
      composition_degree_bound - 3 * trace_length_ + original_trace_length_ + 1,
      composition_degree_bound - trace_length_ + original_trace_length_ - 1,
      composition_degree_bound - trace_length_ + 1};

  // Prepare a list of all the values used in expressions of the form 'gen^value', where gen
  // represents the generator of the trace domain.
  const std::vector<uint64_t> gen_exponents = {original_trace_length_ - 1, res_claim_index_};

  builder.AddPeriodicColumn(PeriodicColumn(kPeriodicValues, trace_length_, slackness_factor_), 0);

  return builder.BuildUniquePtr(
      UseOwned(this), trace_generator, trace_length_, random_coefficients, point_exponents,
      BatchPow(Pow(trace_generator, slackness_factor_), gen_exponents));
}

Trace TestAir::GetTrace(const BaseFieldElement& witness, Prng* prng) const {
  std::vector<std::vector<BaseFieldElement>> trace_values(2);
  trace_values[0].reserve(original_trace_length_);
  BaseFieldElement x = witness;

  for (uint64_t i = 0; i < original_trace_length_; ++i) {
    trace_values[0].push_back(x);
    x = Pow(x, 3);
    trace_values[1].push_back(x);
    x = kConst * x + kPeriodicValues[i % 2];
  }

  Trace trace(std::move(trace_values));
  trace.AddZeroKnowledgeSlackness(slackness_factor_, prng);
  ASSERT_RELEASE(trace.Length() == trace_length_, "Wrong trace length.");
  return trace;
}

BaseFieldElement TestAir::PublicInputFromPrivateInput(
    const BaseFieldElement& witness, const uint64_t res_claim_index) {
  BaseFieldElement x = witness;
  for (uint64_t i = 0; i < res_claim_index; ++i) {
    x = kConst * Pow(x, 3) + kPeriodicValues[i % 2];
  }
  return x;
}

}  // namespace starkware
