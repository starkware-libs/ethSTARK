namespace starkware {

template <typename FieldElementT>
ExtensionFieldElement RescueAir::ConstraintsEval(
    gsl::span<const FieldElementT> neighbors,
    gsl::span<const ExtensionFieldElement> /*composition_neighbors*/,
    gsl::span<const FieldElementT> periodic_columns,
    gsl::span<const ExtensionFieldElement> random_coefficients,
    gsl::span<const FieldElementT> point_powers, gsl::span<const BaseFieldElement> shifts) const {
  using VectorT = std::array<FieldElementT, kStateSize>;
  ASSERT_RELEASE(neighbors.size() == 2 * kStateSize, "Wrong number of neighbors.");
  ASSERT_RELEASE(
      periodic_columns.size() == kNumPeriodicColumns, "Wrong number of periodic column elements.");
  ASSERT_RELEASE(
      random_coefficients.size() == NumRandomCoefficients(),
      "Wrong number of random coefficients.");
  ASSERT_RELEASE(point_powers.size() == 10, "point_powers should contain 10 elements.");
  ASSERT_RELEASE(shifts.size() == 6, "shifts should contain 6 elements.");

  const FieldElementT& point = point_powers[0];

  // domain0 = point^(trace_length / 32) - 1
  // i-th rows for i % 32 == 0, first row of each batch of 3 hashes.
  const FieldElementT& domain0 = point_powers[1] - BaseFieldElement::One();
  // domain1 = point^trace_length - 1 (all rows).
  const FieldElementT& domain1 = point_powers[2] - BaseFieldElement::One();
  // domain2 =
  // (point^(trace_length / 32) - 1) *
  // (point^(trace_length / 32) - gen^(30 * trace_length / 32)) *
  // (point^(trace_length / 32) - gen^(31 * trace_length / 32)).
  // i-th rows for i % 32 == 0,30,31. First, second to last and last rows of each batch of 3 hashes.
  const FieldElementT& domain2 =
      ((point_powers[1] - BaseFieldElement::One()) * (point_powers[1] - shifts[0])) *
      (point_powers[1] - shifts[1]);
  // domain3 =
  // (point^(trace_length / 32) - 1) *
  // (point^(trace_length / 32) - gen^(10 * trace_length / 32)) *
  // (point^(trace_length / 32) - gen^(20 * trace_length / 32)).
  // (point^(trace_length / 32) - gen^(30 * trace_length / 32)) *
  // (point^(trace_length / 32) - gen^(31 * trace_length / 32)) *
  // i-th rows for i % 32 == 0,10,20,30,31
  // First and last rows of each batch and first row of each hash.
  const FieldElementT& domain3 =
      ((((point_powers[1] - BaseFieldElement::One()) * (point_powers[1] - shifts[0])) *
        (point_powers[1] - shifts[1])) *
       (point_powers[1] - shifts[2])) *
      (point_powers[1] - shifts[3]);
  // domain4 =
  // (point^(trace_length / 32) - gen^(10 * trace_length / 32)) *
  // (point^(trace_length / 32) - gen^(20 * trace_length / 32)).
  // i-th rows for i % 32 == 10,20. First row of the second and third hashes in each batch.
  const FieldElementT& domain4 = (point_powers[1] - shifts[2]) * (point_powers[1] - shifts[3]);
  // domain5 = point^(trace_length / 32) - gen^(30 * trace_length / 32).
  // i-th rows for i % 32 == 30. Last row of the third hash in each batch of 3 hashes.
  const FieldElementT& domain5 = point_powers[1] - shifts[0];
  // domain6 = point^(trace_length / 32) - gen^(31 * trace_length / 32)
  // i-th rows for i % 32 == 31. Last row of each batch of 3 hashes.
  const FieldElementT& domain6 = point_powers[1] - shifts[1];
  // domain7 = point - gen^(trace_length - 1). Last row.
  const FieldElementT& domain7 = point - shifts[4];
  // domain8 = point - gen^(32 * (chain_length / 3 - 1) + 31). Output row.
  const FieldElementT& domain8 = point - shifts[5];

  // Compute inverses for the relevant domains.
  const FieldElementT& mult = domain0 * domain1 * domain4 * domain5 * domain6 * domain8;
  const FieldElementT& inv_mult = mult.Inverse();
  const FieldElementT& domain0_inv =
      inv_mult * (domain1 * (domain4 * (domain5 * (domain6 * domain8))));
  const FieldElementT& domain1_inv =
      domain0 * inv_mult * (domain4 * (domain5 * (domain6 * domain8)));
  const FieldElementT& domain4_inv =
      (domain0 * domain1) * inv_mult * (domain5 * (domain6 * domain8));
  const FieldElementT& domain5_inv =
      ((domain0 * domain1) * domain4) * inv_mult * (domain6 * domain8);
  const FieldElementT& domain6_inv =
      (((domain0 * domain1) * domain4) * domain5) * inv_mult * domain8;
  const FieldElementT& domain8_inv =
      ((((domain0 * domain1) * domain4) * domain5) * domain6) * inv_mult;

  // Compute the third powers of the state.
  VectorT x_cube = UninitializedFieldElementArray<FieldElementT, kStateSize>();
  for (size_t i = 0; i < kStateSize; ++i) {
    FieldElementT tmp = neighbors[i];
    x_cube[i] = tmp * tmp * tmp;
  }

  // Compute the state at the end of a full round.
  VectorT state_after_lin_perm = UninitializedFieldElementArray<FieldElementT, kStateSize>();
  for (size_t i = 0; i < kStateSize; ++i) {
    FieldElementT tmp = periodic_columns[i];
    for (size_t j = 0; j < kStateSize; ++j) {
      tmp += (kRescueConstants.k_mds_matrix[i][j] * x_cube[j]);
    }
    state_after_lin_perm[i] = tmp;
  }

  // Compute the state at the beginning of the next full round.
  VectorT state_before_next_lin_perm_cubed =
      UninitializedFieldElementArray<FieldElementT, kStateSize>();
  for (size_t i = 0; i < kStateSize; ++i) {
    FieldElementT tmp = FieldElementT::Zero();
    for (size_t j = 0; j < kStateSize; ++j) {
      tmp += kRescueConstants.k_mds_matrix_inverse[i][j] *
             (neighbors[kStateSize + j] - periodic_columns[kStateSize + j]);
    }
    state_before_next_lin_perm_cubed[i] = tmp * tmp * tmp;
  }

  uint8_t rand_coef_index = 0;
  ExtensionFieldElement res = ExtensionFieldElement::Zero();
  {
    // Compute a sum of constraints for rows i % 32 == 0 (first row of each batch of 3 hashes).
    ExtensionFieldElement sum = ExtensionFieldElement::Zero();

    // Add a constraint that forces the capacity part of the first hash to be zero.
    for (size_t i = 0; i < kWordSize; ++i, rand_coef_index += 2) {
      const FieldElementT constraint = neighbors[2 * kWordSize + i];
      // point_powers[3] = point^degreeAdjustment(composition_degree_bound, trace_length - 1, 0,
      // trace_length / 32).
      const ExtensionFieldElement deg_adj_rand_coef =
          random_coefficients[rand_coef_index] +
          random_coefficients[rand_coef_index + 1] * point_powers[3];
      sum += constraint * deg_adj_rand_coef;
    }

    // Add a constraint that computes the first half round of the first hash and places the result
    // at the second row.
    for (size_t i = 0; i < kStateSize; ++i, rand_coef_index += 2) {
      const FieldElementT constraint =
          neighbors[i] + periodic_columns[i] - state_before_next_lin_perm_cubed[i];
      // point_powers[4] = point^degreeAdjustment(composition_degree_bound, 3 * (trace_length - 1),
      // 0, trace_length / 32).
      const ExtensionFieldElement deg_adj_rand_coef =
          random_coefficients[rand_coef_index] +
          random_coefficients[rand_coef_index + 1] * point_powers[4];
      sum += constraint * deg_adj_rand_coef;
    }
    res += sum * domain0_inv;
  }

  // Constraints that check the consistency between states:
  //
  // Note that the constraints between states inside a hash and between hashes in the same batch are
  // not identical, but are very similar for the first 4 state elements.
  //
  // In the connection between hashes, the capacity is nullified and the second input is reset to
  // some nondeterministic witness.
  //
  // State at the end of the first hash (which corresponds to state_after_lin_perm):
  //    OUTPUT0 | JUNK | JUNK
  //
  // State at the beginning of the second hash (which corresponds to
  // state_before_next_lin_perm_cubed - k_0, where k_0 is the independent round constant that is
  // added before the first round):
  //    INP0 | INP1 | CAPACITY=0
  //
  // To check the consistency between those states, one needs to check that:
  // * OUTPUT0 == INP0
  // * CAPACITY == 0
  //
  // Checking the first item corresponds to checking:
  //   state_after_lin_perm == state_before_next_lin_perm_cubed - k_0.
  // This equation is very similar to the constraints between states inside a hash, which is:
  //   state_after_lin_perm == state_before_next_lin_perm_cubed.
  // In order to reuse the same constraints, we add k_0 to even_round_constants[10] and
  // even_round_constants[20], and thus +k_0 is already part of state_after_lin_perm.
  {
    ExtensionFieldElement sum = ExtensionFieldElement::Zero();
    {
      // Compute a sum of constraints for all rows except i % 32 == 0,30,31 (first, penultimate
      // and last rows of each batch of 3 hashes).
      ExtensionFieldElement inner_sum = ExtensionFieldElement::Zero();

      // Add a constraint that connects the middle of a round (current row) with the middle of the
      // next round (next row) for state[0], ..., state[3].
      for (size_t i = 0; i < kWordSize; ++i, rand_coef_index += 2) {
        const FieldElementT constraint =
            state_after_lin_perm[i] - state_before_next_lin_perm_cubed[i];
        // point_powers[5] = point^degreeAdjustment(composition_degree_bound, 3 * (trace_length -
        // 1), trace_length / 32 + trace_length / 32 + trace_length / 32, trace_length).
        const ExtensionFieldElement deg_adj_rand_coef =
            random_coefficients[rand_coef_index] +
            random_coefficients[rand_coef_index + 1] * point_powers[5];
        inner_sum += constraint * deg_adj_rand_coef;
      }
      sum += inner_sum * domain2;
    }

    {
      // Compute a sum of constraints for all rows except i % 32 == 0,10,20,30,31 (first and last
      // rows of each batch and first row of each hash).
      ExtensionFieldElement inner_sum = ExtensionFieldElement::Zero();

      // Add a constraint that connects the middle of a round (current row) with the middle of the
      // next round (next row) for state[4], ..., state[11].
      for (size_t i = kWordSize; i < kStateSize; ++i, rand_coef_index += 2) {
        const FieldElementT constraint =
            state_after_lin_perm[i] - state_before_next_lin_perm_cubed[i];
        // point_powers[6] = point^degreeAdjustment(composition_degree_bound, 3 * (trace_length -
        // 1), trace_length / 32 + trace_length / 32 + trace_length / 32 + trace_length / 32 +
        // trace_length / 32, trace_length).
        const ExtensionFieldElement deg_adj_rand_coef =
            random_coefficients[rand_coef_index] +
            random_coefficients[rand_coef_index + 1] * point_powers[6];
        inner_sum += constraint * deg_adj_rand_coef;
      }
      sum += inner_sum * domain3;
    }
    res += sum * domain1_inv;
  }

  {
    // Compute a sum of constraints for rows i % 32 == 10,20 (first row of the second and third
    // hashes in each batch).
    ExtensionFieldElement sum = ExtensionFieldElement::Zero();

    // Add a constraint that forces the capacity part of the second and third hashes to be zero.
    // As mentioned in the previos constraint:
    // State before second hash (corresponds to state_before_next_lin_perm_cubed - k_0):
    //   INP0 | INP1 | 0
    // And we require 0 in the capacity range.
    for (size_t i = kStateSize - kWordSize; i < kStateSize; ++i, rand_coef_index += 2) {
      const FieldElementT constraint = periodic_columns[i] - state_before_next_lin_perm_cubed[i];
      // point_powers[7] = point^degreeAdjustment(composition_degree_bound, 3 * (trace_length - 1),
      // 0, trace_length / 32 + trace_length / 32).
      const ExtensionFieldElement deg_adj_rand_coef =
          random_coefficients[rand_coef_index] +
          random_coefficients[rand_coef_index + 1] * point_powers[7];
      sum += constraint * deg_adj_rand_coef;
    }
    res += sum * domain4_inv;
  }

  {
    // Compute a sum of constraints for rows i % 32 == 30 (last row of the third hash in each batch
    // of 3 hashes).
    ExtensionFieldElement sum = ExtensionFieldElement::Zero();

    // Add a constraint that does the final half round of the third hash.
    for (size_t i = 0; i < kStateSize; ++i, rand_coef_index += 2) {
      const FieldElementT constraint = state_after_lin_perm[i] - neighbors[kStateSize + i];
      // point_powers[4] = point^degreeAdjustment(composition_degree_bound, 3 * (trace_length - 1),
      // 0, trace_length / 32).
      const ExtensionFieldElement deg_adj_rand_coef =
          random_coefficients[rand_coef_index] +
          random_coefficients[rand_coef_index + 1] * point_powers[4];
      sum += constraint * deg_adj_rand_coef;
    }
    res += sum * domain5_inv;
  }

  {
    // Compute a sum of constraints for rows i % 32 == 31, i < trace length-1 (last
    // row of each batch of 3 hashes except the last row of the trace).
    ExtensionFieldElement sum = ExtensionFieldElement::Zero();

    // Add a constraint that moves the output of the third hash of a batch to the input of the
    // first hash of the next batch.
    for (size_t i = 0; i < kWordSize; ++i, rand_coef_index += 2) {
      const FieldElementT constraint = neighbors[i] - neighbors[kStateSize + i];
      // point_powers[8] = point^degreeAdjustment(composition_degree_bound, trace_length - 1, 1,
      // trace_length / 32).
      const ExtensionFieldElement deg_adj_rand_coef =
          random_coefficients[rand_coef_index] +
          random_coefficients[rand_coef_index + 1] * point_powers[8];
      sum += constraint * deg_adj_rand_coef;
    }
    sum *= domain7;

    res += sum * domain6_inv;
  }

  {
    // Compute a sum of constraints for the output row.
    ExtensionFieldElement sum = ExtensionFieldElement::Zero();

    // Add a constraint that determines the output after chain_length invocations of the hash.
    for (size_t i = 0; i < kWordSize; ++i, rand_coef_index += 2) {
      const FieldElementT constraint = neighbors[i] - (output_[i]);
      // point_powers[9] = point^degreeAdjustment(composition_degree_bound, trace_length - 1, 0, 1).
      const ExtensionFieldElement deg_adj_rand_coef =
          random_coefficients[rand_coef_index] +
          random_coefficients[rand_coef_index + 1] * point_powers[9];
      sum += constraint * deg_adj_rand_coef;
    }
    res += sum * domain8_inv;
  }
  return res;
}

}  // namespace starkware
