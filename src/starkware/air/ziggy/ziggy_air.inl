namespace starkware {

template <typename FieldElementT>
ExtensionFieldElement ZiggyAir::ConstraintsEval(
    gsl::span<const FieldElementT> neighbors,
    gsl::span<const ExtensionFieldElement> /*composition_neighbors*/,
    gsl::span<const FieldElementT> periodic_columns,
    gsl::span<const ExtensionFieldElement> random_coefficients,
    gsl::span<const FieldElementT> point_powers, gsl::span<const BaseFieldElement> shifts) const {
  using StateVectorT = std::array<FieldElementT, kStateSize>;
  ASSERT_RELEASE(neighbors.size() == 2 * kStateSize, "Wrong number of neighbors.");
  ASSERT_RELEASE(
      periodic_columns.size() == kNumPeriodicColumns, "Wrong number of periodic column elements.");
  ASSERT_RELEASE(
      random_coefficients.size() == NumRandomCoefficients(),
      "Wrong number of random coefficients.");
  ASSERT_RELEASE(point_powers.size() == 3, "point_powers should contain 3 elements.");
  ASSERT_RELEASE(shifts.size() == 10, "shifts should contain 10 elements.");
  static_assert(kNumRounds < kAirHeight, "Number of rounds does not fit into trace size.");
  static_assert(kNumRounds == 10, "Number of rounds is hard coded to 10");

  const FieldElementT& point = point_powers[0];

  // domain0 = point - 1. First row.
  const FieldElementT& domain0 = point - BaseFieldElement::One();
  // domain1 = (point - gen) * (point - gen^2) * (point - gen^3) * (point - gen^4) * (point - gen^5)
  //           * (point - gen^6) * (point - gen^7) * (point - gen^8) * (point - gen^9).
  // Intermediate rows.
  const FieldElementT& domain1 = (point - shifts[0]) * (point - shifts[1]) * (point - shifts[2]) *
                                 (point - shifts[3]) * (point - shifts[4]) * (point - shifts[5]) *
                                 (point - shifts[6]) * (point - shifts[7]) * (point - shifts[8]);
  // domain2 = point - gen^10. Last row.
  const FieldElementT& domain2 = point - shifts[9];

  // Compute inverses for the relevant domains.
  const FieldElementT& mult = domain0 * domain1 * domain2;
  const FieldElementT& inv_mult = mult.Inverse();
  const FieldElementT& domain0_inv = inv_mult * domain1 * domain2;
  const FieldElementT& domain1_inv = domain0 * inv_mult * domain2;
  const FieldElementT& domain2_inv = domain0 * domain1 * inv_mult;

  // Compute the third powers of the state.
  StateVectorT x_cube = UninitializedFieldElementArray<FieldElementT, kStateSize>();
  for (size_t i = 0; i < kStateSize; ++i) {
    FieldElementT tmp = neighbors[i];
    x_cube[i] = tmp * tmp * tmp;
  }

  // Compute the state at the end of a full round.
  StateVectorT state_after_lin_perm = UninitializedFieldElementArray<FieldElementT, kStateSize>();
  for (size_t i = 0; i < kStateSize; ++i) {
    FieldElementT tmp = periodic_columns[i];
    for (size_t j = 0; j < kStateSize; ++j) {
      tmp += (kRescueConstants.k_mds_matrix[i][j] * x_cube[j]);
    }
    state_after_lin_perm[i] = tmp;
  }

  // Compute the state at the beginning of the next full round.
  StateVectorT state_before_next_lin_perm_cubed =
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
    // Compute a sum of constraints for the first row.
    ExtensionFieldElement sum = ExtensionFieldElement::Zero();

    // Add a constraint that forces the capacity part to be zero.
    // The periodic columns are the constant key that is added to the state before the first round.
    // This constraint states that going back half round from the middle of the first round, and
    // then subtracting the constant key gives zero (while it should give the initial state
    // capacity). It refers only to the capacity part because i runs from 2 * kWordSize to
    // kStateSize.
    for (size_t i = 2 * kWordSize; i < kStateSize; ++i, rand_coef_index += 2) {
      const FieldElementT constraint = periodic_columns[i] - state_before_next_lin_perm_cubed[i];
      // point_powers[1] = point^degreeAdjustment(composition_degree_bound, 3 * (trace_length - 1),
      // 0, 1).
      const ExtensionFieldElement deg_adj_rand_coef =
          random_coefficients[rand_coef_index] +
          random_coefficients[rand_coef_index + 1] * point_powers[1];
      sum += constraint * deg_adj_rand_coef;
    }
    res += sum * domain0_inv;
  }

  {
    // Compute a sum of constraints for rows 1,...,9.
    ExtensionFieldElement sum = ExtensionFieldElement::Zero();

    // Add a constraint that connects the middle of a round (current row) with the middle of the
    // next round (next row).
    for (size_t i = 0; i < kStateSize; ++i, rand_coef_index += 2) {
      const FieldElementT constraint =
          state_after_lin_perm[i] - state_before_next_lin_perm_cubed[i];
      // point_powers[2] = point^degreeAdjustment(composition_degree_bound, 3 * (trace_length - 1),
      // 0, 9).
      const ExtensionFieldElement deg_adj_rand_coef =
          random_coefficients[rand_coef_index] +
          random_coefficients[rand_coef_index + 1] * point_powers[2];
      sum += constraint * deg_adj_rand_coef;
    }
    res += sum * domain1_inv;
  }

  {
    // Compute a sum of constraints for the last row.
    ExtensionFieldElement sum = ExtensionFieldElement::Zero();

    // Add a constraint that does the final half round of the hash and determines the output.
    for (size_t i = 0; i < kWordSize; ++i, rand_coef_index += 2) {
      const FieldElementT constraint = state_after_lin_perm[i] - (public_key_[i]);
      // point_powers[1] = point^degreeAdjustment(composition_degree_bound, 3 * (trace_length - 1),
      // 0, 1).
      const ExtensionFieldElement deg_adj_rand_coef =
          random_coefficients[rand_coef_index] +
          random_coefficients[rand_coef_index + 1] * point_powers[1];
      sum += constraint * deg_adj_rand_coef;
    }
    res += sum * domain2_inv;
  }
  return res;
}

}  // namespace starkware
