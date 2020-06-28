#ifndef STARKWARE_COMPOSITION_POLYNOMIAL_BREAKER_H_
#define STARKWARE_COMPOSITION_POLYNOMIAL_BREAKER_H_

#include <memory>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/domains/coset.h"
#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/fields/extension_field_element.h"

namespace starkware {

/*
  Handles "breaking" a polynomial f of degree 2^log_breaks * n, to 2^log_breaks polynomials of
  degree n s.t.
    f(x) = \sum_i x^i h_i(x^(2^log_breaks)).

  coset is the coset in which the input is given.
  The output coset is 2^log_breaks times smaller than the input coset.
  Let x be the offset of the input coset. The offset of the output coset is x^{2^log_breaks}.
*/
class PolynomialBreak {
 public:
  PolynomialBreak(const Coset& coset, size_t log_breaks);
  ~PolynomialBreak() = default;

  /*
    Takes an evaluation of f(x) over a coset, returns all the evaluations of h_i(x)
    for i=0, ..., 2^log_breaks. The coset is specified by the parameters provided in the
    constructor. output is the storage in which the evaluations will be stored, should be the size
    of the coset.
    Returns a vector of 2^log_breaks subspans of output.
  */
  std::vector<gsl::span<const ExtensionFieldElement>> Break(
      const gsl::span<const ExtensionFieldElement>& evaluation,
      gsl::span<ExtensionFieldElement> output) const;

  /*
    Given values of h_i(point) for all 2^log_breaks "broken" polynomials, computes f(point).
  */
  ExtensionFieldElement EvalFromSamples(
      gsl::span<const ExtensionFieldElement> samples, const ExtensionFieldElement& point) const;

 private:
  const Coset coset_;
  const size_t log_breaks_;
};

}  // namespace starkware

#endif  // STARKWARE_COMPOSITION_POLYNOMIAL_BREAKER_H_
