#ifndef STARKWARE_AIR_RESCUE_RESCUE_AIR_UTILS_H_
#define STARKWARE_AIR_RESCUE_RESCUE_AIR_UTILS_H_

#include <array>
#include <utility>
#include <vector>

#include "starkware/air/rescue/rescue_constants.h"
#include "starkware/algebra/fields/base_field_element.h"

namespace starkware {

/*
  An inner state in the computation of the Rescue hash function.
*/
struct RescueState {
  static_assert(
      BaseFieldElement::FieldSize() % 3 == 2, "Base field must not have a third root of unity.");
  static constexpr uint64_t kCubeInverseExponent =
      SafeDiv((2 * BaseFieldElement::FieldSize() - 1), 3);  // = (1/3) % (kModulus - 1).
  static constexpr size_t kStateSize = RescueConstants::kStateSize;

  using VectorT = RescueConstants::VectorT;

  explicit constexpr RescueState(const VectorT& values) : values_(values) {}

  static RescueState Uninitialized() {
    return RescueState(UninitializedFieldElementArray<BaseFieldElement, kStateSize>());
  }

  BaseFieldElement& operator[](size_t i) { return values_.at(i); }
  const BaseFieldElement& operator[](size_t i) const { return values_.at(i); }

  ALWAYS_INLINE RescueState operator*(const RescueState& other) const {
    return RescueState{VectorT{values_[0] * other.values_[0], values_[1] * other.values_[1],
                               values_[2] * other.values_[2], values_[3] * other.values_[3],
                               values_[4] * other.values_[4], values_[5] * other.values_[5],
                               values_[6] * other.values_[6], values_[7] * other.values_[7],
                               values_[8] * other.values_[8], values_[9] * other.values_[9],
                               values_[10] * other.values_[10], values_[11] * other.values_[11]}};
  }

  /*
    Returns the third roots of all field elements within a Rescuestate.
    Follows an optimized calculation of Pow() with exp=kCubeInverseExponent.
  */
  RescueState BatchedThirdRoot() const;

  /*
    Appends this state as a whole row to the trace.
  */
  void PushState(std::vector<std::vector<BaseFieldElement>>* trace_values) const {
    for (size_t i = 0; i < RescueConstants::kStateSize; ++i) {
      trace_values->at(i).push_back(values_[i]);
    }
  }

  RescueState ApplyFirstSBox() const { return BatchedThirdRoot(); }
  RescueState ApplySecondSBox() const { return (*this) * (*this) * (*this); }

  /*
    Applies half a round to the state.
    round_index - a number in the range 0 to 9, indicates which round to apply.
    is_first_half - if true, applies the first half round, else applies the second half round.
  */
  void HalfRound(size_t round_index, bool is_first_half) {
    RescueState state_after_sbox = is_first_half ? ApplyFirstSBox() : ApplySecondSBox();
    LinearTransformation(kRescueConstants.k_mds_matrix, state_after_sbox.values_, &values_);

    const size_t round_constants_index = 2 * round_index + (is_first_half ? 1 : 2);
    for (size_t i = 0; i < RescueConstants::kStateSize; ++i) {
      values_[i] += kRescueConstants.k_round_constants.at(round_constants_index).at(i);
    }
  }

 private:
  VectorT values_;
};

}  // namespace starkware

#endif  // STARKWARE_AIR_RESCUE_RESCUE_AIR_UTILS_H_
