#include "starkware/fri/fri_folder.h"

#include <algorithm>

#include "starkware/utils/task_manager.h"

namespace starkware {
namespace fri {
namespace details {

namespace {

/*
  Let f be the polynomial function of the current FRI layer. Split f to its even and odd terms, g
  and h respectively, f(x) = g(x^2) + xh(x^2).
  Let a be the evaluation point for building the next FRI layer. Define p, the polynomial
  function of the next FRI layer, p = 2g + 2ah. We evaluate p at the point x^2 as
  p(x^2) = f(x) + f(-x) + a(f(x) - f(-x))/x.
  The justification for this calculation is as follow:
  f(x)  = g(x^2) + xh(x^2)
  f(-x) = g((-x)^2) - xh((-x)^2) = g(x^2) - xh(x^2)
  =>
  2g(x^2) = f(x) + f(-x)
  2h(x^2) = (f(x) - f(-x))/x
  =>
  p(x^2) = 2g(x^2) + 2ah(x^2) = f(x) + f(-x) + a(f(x) - f(-x))/x.
*/
ExtensionFieldElement Fold(
    const ExtensionFieldElement& f_x, const ExtensionFieldElement& f_minus_x,
    const ExtensionFieldElement& eval_point, const BaseFieldElement& x_inv) {
  ExtensionFieldElement outcome = f_x + f_minus_x + eval_point * (f_x - f_minus_x) * x_inv;
  return outcome;
}

}  // namespace

std::vector<ExtensionFieldElement> FriFolder::ComputeNextFriLayer(
    const Coset& domain, gsl::span<const ExtensionFieldElement> values,
    const ExtensionFieldElement& eval_point) {
  auto output_layer = ExtensionFieldElement::UninitializedVector(values.size() / 2);
  ComputeNextFriLayer(domain, values, eval_point, output_layer);
  return output_layer;
}

void FriFolder::ComputeNextFriLayer(
    const Coset& domain, gsl::span<const ExtensionFieldElement> values,
    const ExtensionFieldElement& eval_point, gsl::span<ExtensionFieldElement> output_layer) {
  ASSERT_RELEASE(values.size() == domain.Size(), "values size does not match domain size.");
  ASSERT_RELEASE(
      output_layer.size() == SafeDiv(values.size(), 2),
      "Output layer size must be half than the original.");

  // Denote by c the coset offset and by g the generator.
  // domain_vec consists of the inverses of {c, c*g, c*g^2, ..., c*g^(output_layer.size() - 1)},
  // ordered by bit-reverse of the exponent of g^-1.
  // This is half of the inverses of the domain elements, where for each pair x, -x only one of the
  // two appears.
  std::vector<BaseFieldElement> domain_vec = BitReverseVector<BaseFieldElement>(
      Coset(domain.Size(), domain.Generator().Inverse(), domain.Offset().Inverse())
          .GetFirstElements(output_layer.size()));

  for (size_t i = 0; i < output_layer.size(); ++i) {
    const ExtensionFieldElement& f_x = values[2 * i];
    const ExtensionFieldElement& f_minus_x = values[2 * i + 1];
    output_layer[i] = Fold(f_x, f_minus_x, eval_point, domain_vec[i]);
  }
}

ExtensionFieldElement FriFolder::NextLayerElementFromTwoPreviousLayerElements(
    const ExtensionFieldElement& f_x, const ExtensionFieldElement& f_minus_x,
    const ExtensionFieldElement& eval_point, const BaseFieldElement& x) {
  return Fold(f_x, f_minus_x, eval_point, x.Inverse());
}

}  // namespace details
}  // namespace fri
}  // namespace starkware
