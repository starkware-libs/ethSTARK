#ifndef STARKWARE_ALGEBRA_POLYNOMIALS_H_
#define STARKWARE_ALGEBRA_POLYNOMIALS_H_

#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

/*
  Evaluates a polynomials with the given coefficients at a point x.
*/
template <typename FieldElementT>
FieldElementT HornerEval(const FieldElementT& x, const std::vector<FieldElementT>& coefs);

template <typename FieldElementT>
FieldElementT HornerEval(const FieldElementT& x, gsl::span<const FieldElementT> coefs);

/*
  Same as HornerEval(), but for many points.
  Assumption: FieldElementPoints should be an extension field of (or identical to)
  FieldElementCoefs.
*/
template <typename FieldElementPoints, typename FieldElementCoefs>
void BatchHornerEval(
    gsl::span<const FieldElementPoints> points, gsl::span<const FieldElementCoefs> coefs,
    gsl::span<FieldElementPoints> outputs);

}  // namespace starkware

#include "starkware/algebra/polynomials.inl"

#endif  // STARKWARE_ALGEBRA_POLYNOMIALS_H_
