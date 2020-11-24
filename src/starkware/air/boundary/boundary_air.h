#ifndef STARKWARE_AIR_BOUNDARY_BOUNDARY_AIR_H_
#define STARKWARE_AIR_BOUNDARY_BOUNDARY_AIR_H_

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "starkware/air/air.h"

namespace starkware {

/*
  A simple AIR that describes a collection of boundary constraints.
  A boundary constraint has the following form:
    (column_i(x) - y0_i) / (x - x0_i).
  If zero knowledge is enabled, then there is one unconstrained column that is added as is to the
  composition polynomial. This column is filled with random values to mask the composition oracle
  and prevent knowledge leakage.
*/
class BoundaryAir : public Air {
 public:
  using Builder = typename CompositionPolynomialImpl<BoundaryAir>::Builder;

  struct ConstraintData {
    size_t coeff_idx;
    size_t column_index;
    ExtensionFieldElement point_x;
    ExtensionFieldElement point_y;
  };

  /*
    Parameters:
    - trace_length: size of the trace.
    - n_columns: number of columns in the trace.
    - boundary_conditions: list of tuples (column, x, y) that should satisfy column(x)=y.
  */
  BoundaryAir(
      uint64_t trace_length, size_t n_columns,
      gsl::span<const std::tuple<size_t, ExtensionFieldElement, ExtensionFieldElement>>
          boundary_conditions,
      std::optional<size_t> zero_knowledge_column_index)
      : Air(trace_length, /*slackness_factor=*/1),
        trace_length_(trace_length),
        n_columns_(n_columns),
        zero_knowledge_column_index_(zero_knowledge_column_index) {
    constraints_.reserve(boundary_conditions.size());
    size_t coeff_idx = 0;
    // Group boundry conditions by the point_x and store them in constraints_.
    for (const auto& [column_index, point_x, point_y] : boundary_conditions) {
      if (zero_knowledge_column_index_.has_value()) {
        ASSERT_RELEASE(
            column_index != zero_knowledge_column_index_,
            "The blinding column must be unconstrained");
      }
      // Insert the current boundary condition next to one with the same x or at the end of the
      // list.
      auto it = std::find_if(
          constraints_.begin(), constraints_.end(),
          [point_x = point_x](const ConstraintData& constraint) {
            return constraint.point_x == point_x;
          });
      constraints_.insert(it, ConstraintData{coeff_idx, column_index, point_x, point_y});
      coeff_idx++;
    }
    // The mask touches each column once in the current row.
    mask_.reserve(n_columns_);
    for (size_t i = 0; i < n_columns_; ++i) {
      mask_.emplace_back(0, i);
    }
  }

  std::unique_ptr<CompositionPolynomial> CreateCompositionPolynomial(
      const BaseFieldElement& trace_generator,
      gsl::span<const ExtensionFieldElement> random_coefficients) const override {
    Builder builder(0);
    return builder.BuildUniquePtr(
        UseOwned(this), trace_generator, TraceLength(), random_coefficients, {}, {});
  };

  /*
    BoundaryAir does not use periodic_columns and shifts in its ConstraintsEval implementation.
  */
  template <typename FieldElementT>
  ExtensionFieldElement ConstraintsEval(
      gsl::span<const FieldElementT> neighbors,
      gsl::span<const ExtensionFieldElement> composition_neighbors,
      gsl::span<const FieldElementT> /*periodic_columns*/,
      gsl::span<const ExtensionFieldElement> random_coefficients,
      gsl::span<const FieldElementT> point_powers,
      gsl::span<const BaseFieldElement> /*shifts*/) const {
    ASSERT_DEBUG(
        neighbors.size() + composition_neighbors.size() == n_columns_,
        "Wrong number of neighbors.");
    ASSERT_DEBUG(
        random_coefficients.size() == NumRandomCoefficients(),
        "Wrong number of random coefficients.");

    const FieldElementT& point = point_powers[0];

    ExtensionFieldElement outer_sum(ExtensionFieldElement::Zero());
    ExtensionFieldElement inner_sum(ExtensionFieldElement::Zero());

    ExtensionFieldElement prev_x = constraints_[0].point_x;

    for (const ConstraintData& constraint : constraints_) {
      // If the column index is less than neighbors.size(), the neighbor is taken from neighbors,
      // otherwise it is taken from the composition_neighbors (which are regarded as concatenated
      // after neighbors).
      const ExtensionFieldElement neighbor =
          constraint.column_index < neighbors.size()
              ? ExtensionFieldElement(neighbors[constraint.column_index])
              : composition_neighbors[constraint.column_index - neighbors.size()];
      const ExtensionFieldElement constraint_value =
          random_coefficients[constraint.coeff_idx] * (neighbor - constraint.point_y);
      if (prev_x == constraint.point_x) {
        // All constraints with the same constraint.point_x are summed with inner_sum.
        inner_sum += constraint_value;
      } else {
        // New constraint.point_x, add the old (inner_sum/prev_x) to the outer_sum and start a new
        // inner_sum.
        outer_sum += inner_sum / (point - prev_x);
        inner_sum = constraint_value;
        prev_x = constraint.point_x;
      }
    }
    outer_sum += inner_sum / (point - prev_x);

    // Add the unconstrained column as is.
    if (zero_knowledge_column_index_.has_value()) {
      ASSERT_DEBUG(
          zero_knowledge_column_index_ < neighbors.size(),
          "The blinding column is expected to be a base field column.");
      outer_sum += neighbors[*zero_knowledge_column_index_];
    }

    return outer_sum;
  }

  uint64_t TraceLength() const { return trace_length_; }

  uint64_t GetCompositionPolynomialDegreeBound() const override { return TraceLength(); };

  uint64_t NumRandomCoefficients() const override { return constraints_.size(); };

  std::vector<std::pair<int64_t, uint64_t>> GetMask() const override { return mask_; };

  uint64_t NumColumns() const override { return n_columns_; };

 private:
  uint64_t trace_length_;
  size_t n_columns_;
  std::optional<size_t> zero_knowledge_column_index_;
  std::vector<ConstraintData> constraints_;
  std::vector<std::pair<int64_t, uint64_t>> mask_;
};

}  // namespace starkware

#endif  // STARKWARE_AIR_BOUNDARY_BOUNDARY_AIR_H_
