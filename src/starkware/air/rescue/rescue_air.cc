#include "starkware/air/rescue/rescue_air.h"

#include "starkware/air/rescue/rescue_air_utils.h"

namespace starkware {

std::unique_ptr<CompositionPolynomial> RescueAir::CreateCompositionPolynomial(
    const BaseFieldElement& trace_generator,
    gsl::span<const ExtensionFieldElement> random_coefficients) const {
  Builder builder(kNumPeriodicColumns);
  const uint64_t composition_degree_bound = GetCompositionPolynomialDegreeBound();

  // Number of batches, where each batch corresponds to 32 trace lines and 3 hash invocations.
  const uint64_t n_batches = SafeDiv(original_trace_length_, kBatchHeight);
  // Prepare a list of all the values used in expressions of the form 'point^value', where point
  // represents the field elements that will be substituted in the composition polynomial.
  const std::vector<uint64_t> point_exponents = {
      n_batches,
      original_trace_length_,
      composition_degree_bound - trace_length_ + n_batches,
      composition_degree_bound + 2 - (3 * trace_length_) + n_batches,
      composition_degree_bound + 2 - 3 * trace_length_ - 3 * n_batches + original_trace_length_,
      composition_degree_bound + 2 - 3 * trace_length_ - 5 * n_batches + original_trace_length_,
      composition_degree_bound + 2 - (3 * trace_length_) + (2 * n_batches),
      composition_degree_bound - 1 - trace_length_ + n_batches,
      composition_degree_bound - trace_length_ + 1,
  };

  const std::vector<uint64_t> gen_exponents = {
      SafeDiv(15 * (original_trace_length_), 16),
      SafeDiv(31 * (original_trace_length_), 32),
      SafeDiv(5 * (original_trace_length_), 16),
      SafeDiv(5 * (original_trace_length_), 8),
      original_trace_length_ - 1,
      (32 * (SafeDiv(chain_length_, 3) - 1)) + 31,
  };

  BuildPeriodicColumns(&builder);

  return builder.BuildUniquePtr(
      UseOwned(this), trace_generator, trace_length_, random_coefficients, point_exponents,
      BatchPow(Pow(trace_generator, slackness_factor_), gen_exponents));
}

std::vector<std::pair<int64_t, uint64_t>> RescueAir::GetMask() const {
  std::vector<std::pair<int64_t, uint64_t>> mask;
  mask.reserve(2 * kStateSize);
  for (uint64_t row_offset = 0; row_offset < 2; ++row_offset) {
    for (size_t i = 0; i < kStateSize; ++i) {
      mask.emplace_back(row_offset * slackness_factor_, i);
    }
  }
  return mask;
}

void RescueAir::BuildPeriodicColumns(Builder* builder) const {
  // Prepare the round constants with kHashesPerBatch copies each, in the order they are used.
  // There are kNumRounds pairs of constant kStateSize-vectors that are used in each of the
  // kNumRounds rounds of Rescue. The kNumRounds-vector of kStateSize-vectors created by the first
  // element of all the pairs is named even_round_constants, and the kNumRounds-vector of
  // kStateSize-vectors created by the second element of all the pairs is named odd_round_constants.
  // Since there are kHashesPerBatch hash instances in one batch, these constants need to be
  // copied kHashesPerBatch times, once for each hash instance.
  // For more information, see the function ConstraintsEval in the file rescue_air.inl, specifically
  // the comment regarding "constraints that check the consistency between states".

  for (size_t i = 0; i < kStateSize; ++i) {
    std::vector<BaseFieldElement> even_round_constants;
    std::vector<BaseFieldElement> odd_round_constants;
    even_round_constants.reserve(kBatchHeight);
    odd_round_constants.reserve(kBatchHeight);
    // The first element is initialized to zero because of the use of operator+= later on.
    even_round_constants.push_back(BaseFieldElement::Zero());

    for (size_t j = 0; j < kHashesPerBatch; ++j) {
      if (i < kWordSize) {
        even_round_constants.back() += kRescueConstants.k_round_constants[0][i];
      } else {
        even_round_constants.back() = kRescueConstants.k_round_constants[0][i];
      }
      for (size_t round = 0; round < kNumRounds; ++round) {
        odd_round_constants.emplace_back(
            kRescueConstants.k_round_constants.at(2 * round + 1).at(i));
        even_round_constants.emplace_back(
            kRescueConstants.k_round_constants.at(2 * round + 2).at(i));
      }
    }

    // Pad the vectors with zeros to make them of size kBatchHeight.
    even_round_constants.push_back(BaseFieldElement::Zero());
    odd_round_constants.push_back(BaseFieldElement::Zero());
    odd_round_constants.push_back(BaseFieldElement::Zero());
    ASSERT_RELEASE(
        even_round_constants.size() == kBatchHeight, "Wrong length for periodic column.");
    ASSERT_RELEASE(odd_round_constants.size() == kBatchHeight, "Wrong length for periodic column.");
    builder->AddPeriodicColumn(
        PeriodicColumn(even_round_constants, trace_length_, slackness_factor_), i);
    builder->AddPeriodicColumn(
        PeriodicColumn(odd_round_constants, trace_length_, slackness_factor_), kStateSize + i);
  }
}

Trace RescueAir::GetTrace(const WitnessT& witness, Prng* prng) const {
  ASSERT_RELEASE(
      witness.size() == chain_length_ + 1, "Witness size is " + std::to_string(witness.size()) +
                                               ", should be " + std::to_string(chain_length_ + 1) +
                                               ".");
  static_assert(kNumColumns == kStateSize, "Wrong number of columns.");

  std::vector<std::vector<BaseFieldElement>> trace_values(kNumColumns);
  for (auto& column : trace_values) {
    column.reserve(original_trace_length_);
  }

  RescueState state = RescueState::Uninitialized();
  // First witness is the left input for the first hash.
  state[0] = witness[0][0];
  state[1] = witness[0][1];
  state[2] = witness[0][2];
  state[3] = witness[0][3];

  bool output_checked = false;
  for (size_t hash_index = 1;
       hash_index <= kHashesPerBatch * SafeDiv(original_trace_length_, kBatchHeight);) {
    for (size_t hash_index_in_batch = 0; hash_index_in_batch < kHashesPerBatch;
         ++hash_index_in_batch, ++hash_index) {
      state[4] = hash_index < witness.size() ? witness[hash_index][0] : BaseFieldElement::Zero();
      state[5] = hash_index < witness.size() ? witness[hash_index][1] : BaseFieldElement::Zero();
      state[6] = hash_index < witness.size() ? witness[hash_index][2] : BaseFieldElement::Zero();
      state[7] = hash_index < witness.size() ? witness[hash_index][3] : BaseFieldElement::Zero();
      state[8] = BaseFieldElement::Zero();
      state[9] = BaseFieldElement::Zero();
      state[10] = BaseFieldElement::Zero();
      state[11] = BaseFieldElement::Zero();

      if (hash_index_in_batch == 0) {
        // Row 0 is the original state.
        state.PushState(&trace_values);
      }

      for (size_t k = 0; k < kStateSize; ++k) {
        state[k] += kRescueConstants.k_round_constants[0][k];
      }

      for (size_t round = 0; round < RescueConstants::kNumRounds; ++round) {
        state.HalfRound(round, true);
        // We only store the state, midway through the round.
        state.PushState(&trace_values);
        state.HalfRound(round, false);
      }
    }
    // Row 31 is the resulting state of the third hash.
    ASSERT_RELEASE(trace_values[0].size() % 32 == 31, "The current row number is not correct.");
    state.PushState(&trace_values);

    if (hash_index == witness.size()) {
      // Assert that the output equals the expected output.
      for (size_t k = 0; k < kWordSize; ++k) {
        ASSERT_RELEASE(state[k] == output_[k], "Given witness is not a correct preimage.");
      }
      output_checked = true;
    }
  }

  ASSERT_RELEASE(output_checked, "Output correctness was not checked.");
  Trace trace(std::move(trace_values));
  trace.AddZeroKnowledgeSlackness(slackness_factor_, prng);
  ASSERT_RELEASE(trace.Length() == trace_length_, "Wrong trace length.");
  return trace;
}

auto RescueAir::PublicInputFromPrivateInput(const WitnessT& witness) -> WordT {
  ASSERT_RELEASE(
      (witness.size() - 1) % kHashesPerBatch == 0,
      "Incompatible witness size. The number of hash invocations needs to be divisible "
      "by" +
          std::to_string(kHashesPerBatch) + ".");

  RescueState state = RescueState::Uninitialized();
  // First witness is the left input for the first hash.
  state[0] = witness[0][0];
  state[1] = witness[0][1];
  state[2] = witness[0][2];
  state[3] = witness[0][3];

  for (auto witness_iter = witness.begin() + 1; witness_iter != witness.end(); ++witness_iter) {
    state[4] = (*witness_iter)[0];
    state[5] = (*witness_iter)[1];
    state[6] = (*witness_iter)[2];
    state[7] = (*witness_iter)[3];
    state[8] = BaseFieldElement::Zero();
    state[9] = BaseFieldElement::Zero();
    state[10] = BaseFieldElement::Zero();
    state[11] = BaseFieldElement::Zero();

    for (size_t i = 0; i < kStateSize; ++i) {
      state[i] += kRescueConstants.k_round_constants[0][i];
    }

    for (size_t round = 0; round < RescueConstants::kNumRounds; ++round) {
      state.HalfRound(round, true);
      state.HalfRound(round, false);
    }
  }

  return {state[0], state[1], state[2], state[3]};
}

}  // namespace starkware
