#include "starkware/algebra/field_operations.h"

namespace starkware {

template <typename FieldElementT>
FieldElementT HornerEval(const FieldElementT& x, const std::vector<FieldElementT>& coefs) {
  return HornerEval(x, gsl::make_span(coefs));
}

template <typename FieldElementT>
FieldElementT HornerEval(const FieldElementT& x, gsl::span<const FieldElementT> coefs) {
  FieldElementT res = FieldElementT::Uninitialized();
  BatchHornerEval<FieldElementT>({x}, coefs, gsl::make_span(&res, 1));
  return res;
}

template <typename FieldElementPoints, typename FieldElementCoefs>
void BatchHornerEval(
    gsl::span<const FieldElementPoints> points, gsl::span<const FieldElementCoefs> coefs,
    gsl::span<FieldElementPoints> outputs) {
  ASSERT_RELEASE(
      points.size() == outputs.size(),
      "The number of outputs must be the same as the number of points.");
  for (FieldElementPoints& res : outputs) {
    res = FieldElementPoints::Zero();
  }
  for (auto it = coefs.rbegin(); it != coefs.rend(); ++it) {
    for (size_t point_idx = 0; point_idx < points.size(); ++point_idx) {
      outputs[point_idx] = outputs[point_idx] * points[point_idx] + *it;
    }
  }
}

}  // namespace starkware
