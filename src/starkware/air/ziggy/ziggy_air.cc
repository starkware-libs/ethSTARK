#include "starkware/air/ziggy/ziggy_air.h"

namespace starkware {

std::unique_ptr<CompositionPolynomial> ZiggyAir::CreateCompositionPolynomial(
    const BaseFieldElement& trace_generator,
    gsl::span<const ExtensionFieldElement> random_coefficients) const {
  Builder builder(kNumPeriodicColumns);
  const uint64_t composition_degree_bound = GetCompositionPolynomialDegreeBound();

  // Prepare a list of all the values used in expressions of the form 'point^value', where point
  // represents the field elements that will be substituted in the composition polynomial.
  const std::vector<uint64_t> point_exponents = {
      composition_degree_bound + 1 - 3 * (trace_length_ - 1) - 1,
      composition_degree_bound + 9 - 3 * (trace_length_ - 1) - 1,
  };

  const std::vector<uint64_t> gen_exponents = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

  BuildPeriodicColumns(&builder);

  return builder.BuildUniquePtr(
      UseOwned(this), trace_generator, trace_length_, random_coefficients, point_exponents,
      BatchPow(Pow(trace_generator, slackness_factor_), gen_exponents));
}

std::vector<std::pair<int64_t, uint64_t>> ZiggyAir::GetMask() const {
  std::vector<std::pair<int64_t, uint64_t>> mask;
  mask.reserve(2 * kStateSize);
  for (uint64_t row_offset = 0; row_offset < 2; ++row_offset) {
    for (size_t i = 0; i < kStateSize; ++i) {
      mask.emplace_back(row_offset * slackness_factor_, i);
    }
  }
  return mask;
}

void ZiggyAir::BuildPeriodicColumns(Builder* builder) const {
  for (size_t i = 0; i < kStateSize; ++i) {
    std::vector<BaseFieldElement> even_round_constants;
    std::vector<BaseFieldElement> odd_round_constants;
    even_round_constants.reserve(kAirHeight);
    odd_round_constants.reserve(kAirHeight);
    even_round_constants.push_back(kRescueConstants.k_round_constants[0][i]);
    for (size_t round = 0; round < kNumRounds; ++round) {
      odd_round_constants.push_back(kRescueConstants.k_round_constants.at(2 * round + 1).at(i));
      even_round_constants.push_back(kRescueConstants.k_round_constants.at(2 * round + 2).at(i));
    }
    for (size_t j = kNumRounds; j < kAirHeight; ++j) {
      odd_round_constants.push_back(BaseFieldElement::Zero());
    }
    for (size_t j = kNumRounds + 1; j < kAirHeight; ++j) {
      even_round_constants.push_back(BaseFieldElement::Zero());
    }
    ASSERT_RELEASE(even_round_constants.size() == kAirHeight, "Wrong length for periodic column");
    ASSERT_RELEASE(odd_round_constants.size() == kAirHeight, "Wrong length for periodic column");
    builder->AddPeriodicColumn(
        PeriodicColumn(even_round_constants, trace_length_, slackness_factor_), i);
    builder->AddPeriodicColumn(
        PeriodicColumn(odd_round_constants, trace_length_, slackness_factor_), kStateSize + i);
  }
}

Trace ZiggyAir::GetTrace(const SecretPreimageT& secret_preimage, Prng* prng) const {
  static_assert(kNumColumns == kStateSize, "Wrong number of columns.");

  std::vector<std::vector<BaseFieldElement>> trace_values(kNumColumns);
  for (auto& column : trace_values) {
    column.reserve(kAirHeight);
  }
  constexpr BaseFieldElement kZero = BaseFieldElement::Zero();
  constexpr RescueState kZeroState(
      {kZero, kZero, kZero, kZero, kZero, kZero, kZero, kZero, kZero, kZero, kZero, kZero});
  kZeroState.PushState(&trace_values);

  RescueState state({secret_preimage[0], secret_preimage[1], secret_preimage[2], secret_preimage[3],
                     secret_preimage[4], secret_preimage[5], secret_preimage[6], secret_preimage[7],
                     BaseFieldElement::Zero(), BaseFieldElement::Zero(), BaseFieldElement::Zero(),
                     BaseFieldElement::Zero()});

  for (size_t k = 0; k < kStateSize; ++k) {
    state[k] += kRescueConstants.k_round_constants[0][k];
  }

  for (size_t round = 0; round < RescueConstants::kNumRounds; ++round) {
    state.HalfRound(round, true);
    // We only store the state, midway through the round.
    state.PushState(&trace_values);
    state.HalfRound(round, false);
  }
  for (size_t k = 0; k < kWordSize; ++k) {
    ASSERT_RELEASE(state[k] == public_key_[k], "Given secret preimage is not a correct preimage.");
  }

  for (size_t row = RescueConstants::kNumRounds + 1; row < kAirHeight; ++row) {
    kZeroState.PushState(&trace_values);
  }

  Trace trace(std::move(trace_values));
  trace.AddZeroKnowledgeSlackness(slackness_factor_, prng);
  ASSERT_RELEASE(trace.Length() == trace_length_, "Wrong trace length.");
  return trace;
}

auto ZiggyAir::PublicInputFromPrivateInput(const SecretPreimageT& secret_preimage)
    -> PublicKeyT {
  RescueState state({secret_preimage[0], secret_preimage[1], secret_preimage[2], secret_preimage[3],
                     secret_preimage[4], secret_preimage[5], secret_preimage[6], secret_preimage[7],
                     BaseFieldElement::Zero(), BaseFieldElement::Zero(), BaseFieldElement::Zero(),
                     BaseFieldElement::Zero()});

  for (size_t i = 0; i < kStateSize; ++i) {
    state[i] += kRescueConstants.k_round_constants[0][i];
  }

  for (size_t round = 0; round < RescueConstants::kNumRounds; ++round) {
    state.HalfRound(round, true);
    state.HalfRound(round, false);
  }

  return {state[0], state[1], state[2], state[3]};
}

}  // namespace starkware
