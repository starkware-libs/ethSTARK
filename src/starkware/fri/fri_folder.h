#ifndef STARKWARE_FRI_FRI_FOLDER_H_
#define STARKWARE_FRI_FRI_FOLDER_H_

#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/domains/coset.h"
#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/fields/extension_field_element.h"

namespace starkware {
namespace fri {
namespace details {

/*
  Performs the "FRI formula", folding current layer into the next.
*/
class FriFolder {
 public:
  /*
    Computes the values of the next FRI layer given the values and domain of the current layer.
    values should be ordered by bit-reverse.
  */
  static std::vector<ExtensionFieldElement> ComputeNextFriLayer(
      const Coset& domain, gsl::span<const ExtensionFieldElement> values,
      const ExtensionFieldElement& eval_point);

  static void ComputeNextFriLayer(
      const Coset& domain, gsl::span<const ExtensionFieldElement> values,
      const ExtensionFieldElement& eval_point, gsl::span<ExtensionFieldElement> output_layer);

  /*
    Computes the value of a single element in the next FRI layer given two corresponding
    elements in the current layer.
  */
  static ExtensionFieldElement NextLayerElementFromTwoPreviousLayerElements(
      const ExtensionFieldElement& f_x, const ExtensionFieldElement& f_minus_x,
      const ExtensionFieldElement& eval_point, const BaseFieldElement& x);
};

}  // namespace details
}  // namespace fri
}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_FOLDER_H_
