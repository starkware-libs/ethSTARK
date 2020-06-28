#ifndef STARKWARE_ALGEBRA_LDE_LDE_MANAGER_H_
#define STARKWARE_ALGEBRA_LDE_LDE_MANAGER_H_

#include <memory>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/domains/coset.h"
#include "starkware/utils/task_manager.h"

namespace starkware {

/*
  Manages several columns' FFT invocations over the trace domain.
  The trace domain is defined by the coset argument. Columns are added by invoking AddEvaluation().
*/
template <typename FieldElementT>
class LdeManager {
 public:
  explicit LdeManager(const Coset& coset, bool eval_in_natural_order);

  virtual ~LdeManager() = default;

  /*
    Adds an evaluation on the coset that was used to build the LdeManager.
    Future EvalOnCoset invocations will add the lde of that evaluation to the results.
  */
  virtual void AddEvaluation(std::vector<FieldElementT>&& evaluation);
  virtual void AddEvaluation(gsl::span<const FieldElementT> evaluation);

  /*
    Evaluates the low degree extension of the evaluations that were previously added
    on a given coset.
    The results are ordered according to the order that the LDE columns were added.
  */
  virtual void EvalOnCoset(
      const BaseFieldElement& coset_offset,
      gsl::span<const gsl::span<FieldElementT>> evaluation_results,
      TaskManager* task_manager) const;

  virtual void EvalOnCoset(
      const BaseFieldElement& coset_offset,
      gsl::span<const gsl::span<FieldElementT>> evaluation_results) const;

  /*
    Constructs an LDE from the coefficients of the polynomial (as obtained by GetCoefficients()).
  */
  void AddFromCoefficients(gsl::span<const FieldElementT> coefficients);

  /*
    Evaluates all columns at the given points.
  */
  template <typename ExtFieldElementT>
  void EvalAtPoints(
      size_t evaluation_idx, gsl::span<const ExtFieldElementT> points,
      gsl::span<ExtFieldElementT> outputs) const;

  /*
    Returns the degree of the interpolation polynomial for an evaluation that was previously added
    through AddEvaluation().

    Returns -1 for the zero polynomial.
  */
  int64_t GetEvaluationDegree(size_t evaluation_idx) const;

  /*
    Returns the coefficients of the interpolation polynomial.
    Note that the returned span is only valid while the lde_manger is alive.
  */
  gsl::span<const FieldElementT> GetCoefficients(size_t evaluation_idx) const;

  /*
    Returns the size of the trace domain.
  */
  virtual uint64_t GetDomainSize() const;

  /*
    Returns true if the evaluation is naturally ordered, false if it is bit-reversed ordered.
    For additional details see eval_in_natural_order_ below.
  */
  bool IsEvalNaturallyOrdered() const;

 private:
  // A coset representing the trace domain.
  const Coset coset_;

  // Corresponds to the order of the coefficients in the interpolation polynomial.
  // If eval_in_natural_order_ is true then the evaluation is in natural order and the coefficients
  // are in bit-reversed order. Otherwise, the evaluation is in bit-reversed order and the
  // coefficients are in natural order.
  const bool eval_in_natural_order_;

  // Holds the interpolation polynomials.
  std::vector<std::vector<FieldElementT>> polynomials_vector_;

  // If eval_in_natural_order_ is true, polynomials_natural_order_coefficients_vector_ holds the
  // interpolation polynomials with natural ordered coeffcients. Otherwise, this vector is empty.
  std::vector<std::vector<FieldElementT>> polynomials_natural_order_coefficients_vector_;
};

template <typename FieldElementT>
std::unique_ptr<LdeManager<FieldElementT>> MakeLdeManager(
    const Coset& source_domain_coset, bool eval_in_natural_order = true);

}  // namespace starkware

#include "starkware/algebra/lde/lde_manager.inl"

#endif  // STARKWARE_ALGEBRA_LDE_LDE_MANAGER_H_
