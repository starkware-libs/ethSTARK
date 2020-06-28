namespace starkware {

template <typename FieldElementT>
ExtensionFieldElement TestAir::ConstraintsEval(
    gsl::span<const FieldElementT> neighbors,
    gsl::span<const ExtensionFieldElement> /*composition_neighbors*/,
    gsl::span<const FieldElementT> periodic_columns,
    gsl::span<const ExtensionFieldElement> random_coefficients,
    gsl::span<const FieldElementT> point_powers, gsl::span<const BaseFieldElement> shifts) const {
  ASSERT_RELEASE(neighbors.size() == kNumNeighbors, "Wrong number of neighbors.");
  ASSERT_RELEASE(periodic_columns.size() == 1, "Wrong number of periodic column elements.");
  ASSERT_RELEASE(
      random_coefficients.size() == NumRandomCoefficients(),
      "Wrong number of random coefficients.");
  ASSERT_RELEASE(point_powers.size() == 5, "point_powers should contain 5 elements.");
  ASSERT_RELEASE(shifts.size() == 2, "shifts should contain 2 elements.");
  const FieldElementT& point = point_powers[0];

  // domain_all_rows = point^trace_length - 1 (all rows).
  const FieldElementT& domain_all_rows = point_powers[1] - BaseFieldElement::One();
  // domain_last_row = point - gen^(trace_length - 1) (last row).
  const FieldElementT& domain_last_row = point - shifts[0];
  // domain_claim_index_row = point - gen^res_claim_index (res_claim_index row).
  const FieldElementT& domain_claim_index_row = point - shifts[1];

  ASSERT_RELEASE(neighbors.size() == 3, "Neighbors must contain 3 elements.");
  const FieldElementT& x_row0 = neighbors[0];
  const FieldElementT& x_row1 = neighbors[1];
  const FieldElementT& y_row0 = neighbors[2];

  ASSERT_RELEASE(periodic_columns.size() == 1, "periodic_columns should contain 1 element.");

  ExtensionFieldElement res(ExtensionFieldElement::Zero());
  {
    // Compute a sum of constraints for all but last row.
    ExtensionFieldElement sum = ExtensionFieldElement::Zero();
    {
      // Constraint expression for raising to the third power (y_i = x_i**3).
      const FieldElementT constraint = x_row0 * x_row0 * x_row0 - y_row0;
      // point_powers[2] = point^degreeAdjustment(composition_degree_bound, 3 * (trace_length - 1),
      // 1, trace_length).
      const ExtensionFieldElement deg_adj =
          random_coefficients[0] + random_coefficients[1] * point_powers[2];
      sum += constraint * deg_adj;
    }
    {
      // Constraint expression for consecutive rows (x_{i+1} = 16 * y_i + periodic).
      const FieldElementT constraint = kConst * y_row0 + periodic_columns[0] - x_row1;
      // point_powers[3] = point^degreeAdjustment(composition_degree_bound, trace_length - 1, 1,
      // trace_length).
      const ExtensionFieldElement deg_adj =
          random_coefficients[2] + random_coefficients[3] * point_powers[3];
      sum += constraint * deg_adj;
    }
    res += sum * domain_last_row / domain_all_rows;
  }

  {
    // Compute a sum of constraints for res_claim_index-th row.
    ExtensionFieldElement sum = ExtensionFieldElement::Zero();

    // Constraint expression for the claim (res_claim_index-th element is claimed_res).
    const FieldElementT constraint = x_row0 - claimed_res_;
    // point_powers[4] = point^degreeAdjustment(composition_degree_bound, trace_length - 1, 0, 1).
    const ExtensionFieldElement deg_adj =
        random_coefficients[4] + random_coefficients[5] * point_powers[4];
    sum += constraint * deg_adj;

    res += sum / domain_claim_index_row;
  }
  return res;
}

}  // namespace starkware
