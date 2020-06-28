#include "starkware/air/rescue/rescue_constants.h"

#include "gtest/gtest.h"

namespace starkware {
namespace {

TEST(RescueConstantsTest, MdsMatrixInverseTest) {
  for (size_t i = 0; i < RescueConstants::kStateSize; ++i) {
    for (size_t j = 0; j < RescueConstants::kStateSize; ++j) {
      BaseFieldElement sum = BaseFieldElement::Zero();
      for (size_t k = 0; k < RescueConstants::kStateSize; ++k) {
        sum += kRescueConstants.k_mds_matrix[i][k] * kRescueConstants.k_mds_matrix_inverse[k][j];
      }
      ASSERT_EQ(sum, i == j ? BaseFieldElement::One() : BaseFieldElement::Zero());
    }
  }
}

}  // namespace
}  // namespace starkware
