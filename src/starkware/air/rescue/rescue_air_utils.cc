#include "starkware/air/rescue/rescue_air_utils.h"

namespace starkware {

RescueState RescueState::BatchedThirdRoot() const {
  // Efficiently computes the third root (i.e. base ^ kCubeInverseExponent, where
  // kCubeInverseExponent = 0b1010101010101010101010110001010101010101010101010101010101011) by
  // using 68 multiplications.

  const RescueState& base = *this;

  // Computes base ^ 0b10.
  const RescueState temp01 = base * base;
  // Computes base ^ 0b100.
  const RescueState temp02 = temp01 * temp01;
  // Computes base ^ 0b1000.
  const RescueState temp03 = temp02 * temp02;

  // Computes base ^ 0b1010.
  const RescueState temp04 = temp03 * temp01;

  // Computes base ^ 0b10100.
  const RescueState temp05 = temp04 * temp04;
  // Computes base ^ 0b101000.
  const RescueState temp06 = temp05 * temp05;
  // Computes base ^ 0b1010000.
  const RescueState temp07 = temp06 * temp06;
  // Computes base ^ 0b10100000.
  const RescueState temp08 = temp07 * temp07;

  // Computes base ^ 0b10101010.
  const RescueState temp09 = temp08 * temp04;

  // Computes base ^ 0b1010101000000000.
  RescueState prev = temp09;
  for (size_t i = 9; i < 17; ++i) {
    prev = prev * prev;
  }
  const RescueState temp17 = prev;

  // Computes base ^ 0b1010101010101010.
  const RescueState temp18 = temp17 * temp09;

  // Computes base ^ 0b101010101010101000000000.
  prev = temp18;
  for (size_t i = 18; i < 26; ++i) {
    prev = prev * prev;
  }
  const RescueState temp26 = prev;

  // Computes base ^ 0b101010101010101010101010.
  const RescueState temp27 = temp26 * temp09;
  // Computes base ^ 0b101010101010101010101011.
  const RescueState temp28 = temp27 * base;

  // Computes base ^ 0b10101010101010101010101100000000000.
  prev = temp28;
  for (size_t i = 28; i < 39; ++i) {
    prev = prev * prev;
  }
  const RescueState temp39 = prev;

  // Computes base ^ 0b10101010101010101010101100010101010.
  const RescueState temp40 = temp39 * temp09;
  // Computes base ^ 0b101010101010101010101011000101010100.
  const RescueState temp41 = temp40 * temp40;
  // Computes base ^ 0b1010101010101010101010110001010101000.
  const RescueState temp42 = temp41 * temp41;
  // Computes base ^ 0b1010101010101010101010110001010101010.
  const RescueState temp43 = temp42 * temp01;

  // Computes base ^ 0b1010101010101010101010110001010101010000000000000000000000000.
  prev = temp43;
  for (size_t i = 43; i < 67; ++i) {
    prev = prev * prev;
  }
  const RescueState temp67 = prev;

  // Returns base ^ 0b1010101010101010101010110001010101010101010101010101010101011.
  return temp67 * temp28;
}

}  // namespace starkware
