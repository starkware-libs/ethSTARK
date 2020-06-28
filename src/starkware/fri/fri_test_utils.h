#ifndef STARKWARE_FRI_FRI_TEST_UTILS_H_
#define STARKWARE_FRI_FRI_TEST_UTILS_H_

#include <vector>

#include "starkware/algebra/domains/coset.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/fri/fri_details.h"

namespace starkware {

namespace fri {
namespace details {

struct TestPolynomial {
  TestPolynomial(Prng* prng, int degree_bound) {
    coefs = prng->RandomFieldElementVector<ExtensionFieldElement>(degree_bound - 1);
    // Make the last coefficient different from zero, so the polynomial is of degree ==
    // degree_bound - 1.
    coefs.push_back(RandomNonZeroElement<ExtensionFieldElement>(prng));
  }

  /*
    Evaluates the polynomial at x.
  */
  ExtensionFieldElement EvalAt(ExtensionFieldElement x) const { return HornerEval(x, coefs); }

  /*
    Evaluates the polynomial on the given domain.
  */
  std::vector<ExtensionFieldElement> GetData(const Coset& domain) const {
    std::vector<ExtensionFieldElement> vec;
    vec.reserve(domain.Size());
    for (const BaseFieldElement& x :
         domain.GetElements(MultiplicativeGroupOrdering::kBitReversedOrder)) {
      vec.push_back(EvalAt(ExtensionFieldElement(x)));
    }
    return vec;
  }

  std::vector<ExtensionFieldElement> coefs;
};

}  // namespace details
}  // namespace fri

/*
  Given an evaluation of a polynomial on a coset, evaluates at a given point.
*/
inline ExtensionFieldElement ExtrapolatePoint(
    const Coset& domain, const std::vector<ExtensionFieldElement>& vec,
    const ExtensionFieldElement eval_point) {
  auto lde_manager = MakeLdeManager<ExtensionFieldElement>(domain, false);
  lde_manager->AddEvaluation(vec);
  auto evaluation_result = ExtensionFieldElement::Uninitialized();
  lde_manager->EvalAtPoints(
      0, gsl::make_span(&eval_point, 1), gsl::make_span(&evaluation_result, 1));
  return evaluation_result;
}

/*
  Given the coefficients of a polynomial, evaluates it at a given point.
*/
template <typename FieldElementT>
FieldElementT ExtrapolatePointFromCoefficients(
    const Coset& domain, const std::vector<FieldElementT>& orig_coefs,
    const FieldElementT eval_point) {
  auto lde_manager = MakeLdeManager<ExtensionFieldElement>(domain, false);
  std::vector<FieldElementT> coefs(orig_coefs);
  ASSERT_RELEASE(coefs.size() <= domain.Size(), "Too many coefficients");
  coefs.resize(domain.Size(), FieldElementT::Zero());

  lde_manager->AddFromCoefficients(gsl::span<const FieldElementT>(coefs));
  auto evaluation_result = FieldElementT::Uninitialized();
  lde_manager->EvalAtPoints(
      0, gsl::make_span(&eval_point, 1), gsl::make_span(&evaluation_result, 1));
  return evaluation_result;
}

}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_TEST_UTILS_H_
