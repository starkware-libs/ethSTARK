#include "starkware/air/rescue/rescue_air_utils.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace starkware {
namespace {

TEST(RescueAirUtils, BatchedThirdRoot) {
  Prng prng;

  RescueState state({
      BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng),
      BaseFieldElement::RandomElement(&prng),
  });

  const RescueState batched_third_roots = state.BatchedThirdRoot();

  for (size_t i = 0; i < RescueState::kStateSize; ++i) {
    EXPECT_EQ(batched_third_roots[i], Pow(state[i], RescueState::kCubeInverseExponent));
    EXPECT_EQ(Pow(batched_third_roots[i], 3), state[i]);
  }
}

}  // namespace
}  // namespace starkware
