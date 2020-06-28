#ifndef STARKWARE_COMPOSITION_POLYNOMIAL_COMPOSITION_POLYNOMIAL_H_
#define STARKWARE_COMPOSITION_POLYNOMIAL_COMPOSITION_POLYNOMIAL_H_

#include <memory>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/composition_polynomial/neighbors.h"
#include "starkware/composition_polynomial/periodic_column.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

/*
  Represents a polynomial of the form:

  F(x, y_1, y_2, ... , y_k) =
  \sum_i (c_{2*i} + c_{2*i+1} * x^{n_i}) * f_i(x, y_0, y_1, ... , y_k, p_0, ... , p_m) *
  P_i(x)/Q_i(x).

  Where:

  - The term (c_{2*i} + c_{2*i+1} * x^{n_i}) is used for both logical 'AND' over all the
    constraints, and degree adjustment of each constraint.
  - The sequence (p_0, ... , p_m) consists of the evaluations of the periodic public columns.
  - The term f_i(y_0, y_1, ... , y_k, p_0, ... , p_m) represents a constraint.
  - The term P_i(x)/Q_i(x) is a rational function such that Q_i(x)/P(i) is a polynomial with only
    simple roots, and its roots are exactly the locations in which the constraint f_i has to be
    satisfied.

  Parameters deduction:

  - (c_0, c_1, ...) are the random coefficients chosen by the verifier.
  - The values of (n_0, n_1,...) are computed so that the degree bound of the resulting polynomial
    will be air->GetCompositionPolynomialDegreeBound().
  - The functions (f_0, f_1,...) are induced by air.ConstraintsEval().
  - The mask (for evaluations on entire cosets) is obtained from air.GetMask().

  This class is used both to evaluate F(x, y_0, y_1, ...) on a single point, and on entire cosets
  using optimizations improving the (amortized) computation time for each point in the coset.
*/
class CompositionPolynomial {
 public:
  virtual ~CompositionPolynomial() = default;

  /*
    Evaluates the composition polynomial at a single point. The neighbors are the values obtained
    from the trace's low degree extension, using the AIR's mask.
  */
  virtual ExtensionFieldElement EvalAtPoint(
      const BaseFieldElement& point, gsl::span<const BaseFieldElement> neighbors,
      gsl::span<const ExtensionFieldElement> composition_neighbors) const = 0;

  virtual ExtensionFieldElement EvalAtPoint(
      const ExtensionFieldElement& point, gsl::span<const ExtensionFieldElement> neighbors,
      gsl::span<const ExtensionFieldElement> composition_neighbors) const = 0;

  /*
    Evaluates the composition polynomial on the coset coset_offset*<group_generator>, which must be
    of size coset_size. The evaluation is split into different tasks of size task_size each.
    The evaluation is written to 'out_evaluation', in bit-reversed order: out_evaluation[i] contains
    the evaluation on the point coset_offset*(group_generator^{bit_reverse(i)}).
  */
  virtual void EvalOnCosetBitReversedOutput(
      const BaseFieldElement& coset_offset,
      gsl::span<const gsl::span<const BaseFieldElement>> trace_lde,
      gsl::span<const gsl::span<const ExtensionFieldElement>> composition_trace_lde,
      gsl::span<ExtensionFieldElement> out_evaluation, uint64_t task_size) const = 0;

  /*
    Returns the degree bound of the composition polynomial.
  */
  virtual uint64_t GetDegreeBound() const = 0;
};

template <typename AirT>
class CompositionPolynomialImpl : public CompositionPolynomial {
 public:
  class Builder {
   public:
    explicit Builder(uint64_t num_periodic_columns) : periodic_columns_(num_periodic_columns) {}

    void AddPeriodicColumn(PeriodicColumn column, size_t periodic_column_index);

    /*
      Builds an instance of CompositionPolynomialImpl.
      Note that once Build or BuildUniquePtr are used, the periodic columns that were added
      previously are consumed and the Builder goes back to a clean slate state.
    */
    CompositionPolynomialImpl Build(
        MaybeOwnedPtr<const AirT> air, const BaseFieldElement& trace_generator, uint64_t coset_size,
        gsl::span<const ExtensionFieldElement> random_coefficients,
        gsl::span<const uint64_t> point_exponents, gsl::span<const BaseFieldElement> shifts);

    std::unique_ptr<CompositionPolynomialImpl<AirT>> BuildUniquePtr(
        MaybeOwnedPtr<const AirT> air, const BaseFieldElement& trace_generator, uint64_t coset_size,
        gsl::span<const ExtensionFieldElement> random_coefficients,
        gsl::span<const uint64_t> point_exponents, gsl::span<const BaseFieldElement> shifts);

   private:
    std::vector<std::optional<PeriodicColumn>> periodic_columns_;
  };

  /*
    For performance reasons we have separate functions for evaluating at a BaseFieldElement point
    and an ExtensionFieldElement point, although we could use the ExtensionFieldElement version for
    both cases by casting.
  */
  ExtensionFieldElement EvalAtPoint(
      const BaseFieldElement& point, gsl::span<const BaseFieldElement> neighbors,
      gsl::span<const ExtensionFieldElement> composition_neighbors) const override {
    return EvalAtPointImpl(point, neighbors, composition_neighbors);
  }

  ExtensionFieldElement EvalAtPoint(
      const ExtensionFieldElement& point, gsl::span<const ExtensionFieldElement> neighbors,
      gsl::span<const ExtensionFieldElement> composition_neighbors) const override {
    return EvalAtPointImpl(point, neighbors, composition_neighbors);
  }

  template <typename FieldElementT>
  ExtensionFieldElement EvalAtPointImpl(
      const FieldElementT& point, gsl::span<const FieldElementT> neighbors,
      gsl::span<const ExtensionFieldElement> composition_neighbors) const;

  void EvalOnCosetBitReversedOutput(
      const BaseFieldElement& coset_offset,
      gsl::span<const gsl::span<const BaseFieldElement>> trace_lde,
      gsl::span<const gsl::span<const ExtensionFieldElement>> composition_trace_lde,
      gsl::span<ExtensionFieldElement> out_evaluation, uint64_t task_size) const override;

  /*
    Same as above where neighbors are the values obtained from the trace low degree extension, using
    the AIR's mask.
  */
  void EvalOnCosetBitReversedOutput(
      const BaseFieldElement& coset_offset, const Neighbors& neighbors,
      gsl::span<ExtensionFieldElement> out_evaluation, uint64_t task_size) const;

  uint64_t GetDegreeBound() const override { return air_->GetCompositionPolynomialDegreeBound(); }

 private:
  /*
    The constructor is private.
    Users should use the Builder class to build an instance of this class.
  */
  CompositionPolynomialImpl(
      MaybeOwnedPtr<const AirT> air, BaseFieldElement trace_generator, uint64_t coset_size,
      std::vector<PeriodicColumn> periodic_columns,
      gsl::span<const ExtensionFieldElement> coefficients,
      gsl::span<const uint64_t> point_exponents, gsl::span<const BaseFieldElement> shifts);

  MaybeOwnedPtr<const AirT> air_;
  const BaseFieldElement trace_generator_;
  const uint64_t coset_size_;
  const std::vector<PeriodicColumn> periodic_columns_;
  const std::vector<ExtensionFieldElement> coefficients_;
  // Exponents of the point powers that are needed for the evaluation of the composition polynomial.
  const std::vector<uint64_t> point_exponents_;
  // Powers of the generator needed for the evaluation of the composition polynomial.
  const std::vector<BaseFieldElement> shifts_;
};

}  // namespace starkware

#include "starkware/composition_polynomial/composition_polynomial.inl"

#endif  // STARKWARE_COMPOSITION_POLYNOMIAL_COMPOSITION_POLYNOMIAL_H_
