#ifndef STARKWARE_COMPOSITION_POLYNOMIAL_COMPOSITION_POLYNOMIAL_MOCK_H_
#define STARKWARE_COMPOSITION_POLYNOMIAL_COMPOSITION_POLYNOMIAL_MOCK_H_

#include "gmock/gmock.h"

#include "starkware/composition_polynomial/composition_polynomial.h"

namespace starkware {

class CompositionPolynomialMock : public CompositionPolynomial {
 public:
  MOCK_CONST_METHOD3(
      EvalAtPoint, ExtensionFieldElement(
                       const BaseFieldElement&, gsl::span<const BaseFieldElement>,
                       gsl::span<const ExtensionFieldElement>));
  MOCK_CONST_METHOD3(
      EvalAtPoint, ExtensionFieldElement(
                       const ExtensionFieldElement&, gsl::span<const ExtensionFieldElement>,
                       gsl::span<const ExtensionFieldElement>));
  MOCK_CONST_METHOD5(
      EvalOnCosetBitReversedOutput,
      void(
          const BaseFieldElement&, gsl::span<const gsl::span<const BaseFieldElement>>,
          gsl::span<const gsl::span<const ExtensionFieldElement>>, gsl::span<ExtensionFieldElement>,
          uint64_t));
  MOCK_CONST_METHOD0(GetDegreeBound, uint64_t());
};

}  // namespace starkware

#endif  // STARKWARE_COMPOSITION_POLYNOMIAL_COMPOSITION_POLYNOMIAL_MOCK_H_
