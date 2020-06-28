#ifndef STARKWARE_ALGEBRA_FIELD_ELEMENT_BASE_H_
#define STARKWARE_ALGEBRA_FIELD_ELEMENT_BASE_H_

#include <iostream>
#include <vector>

#include "starkware/utils/attributes.h"

namespace starkware {

/*
  A non-virtual base class for field elements (using CRTP). This class provides some common
  field-agnostic functionality.
  For CRTP, see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern.

  To define a field element that has the functionality provided by this class, write:
    class MyFieldElement : public FieldElementBase<MyFieldElement> {
      ...
    };
*/
template <typename Derived>
class FieldElementBase {
 public:
  Derived& operator+=(const Derived& other);
  ALWAYS_INLINE Derived& operator*=(const Derived& other);
  Derived operator/(const Derived& other) const;
  constexpr bool operator!=(const Derived& other) const;

  static std::vector<Derived> UninitializedVector(size_t size) {
#ifdef NDEBUG
    return std::vector<Derived>(size);  // for faster memory allocation.
#else
    return std::vector<Derived>(size, Derived::Uninitialized());
#endif
  }

  constexpr const Derived& AsDerived() const { return static_cast<const Derived&>(*this); }
  constexpr Derived& AsDerived() { return static_cast<Derived&>(*this); }
};

/*
  This constant is true if FieldElementT is a field and false otherwise.
  For example, kIsFieldElement<BaseFieldElement> == true and kIsFieldElement<int> == false.
*/
template <typename FieldElementT>
constexpr bool kIsFieldElement =
    std::is_base_of<FieldElementBase<FieldElementT>, FieldElementT>::value;

template <typename FieldElementT>
std::enable_if_t<kIsFieldElement<FieldElementT>, std::ostream&> operator<<(
    std::ostream& out, const FieldElementT& element);

}  // namespace starkware

#include "starkware/algebra/field_element_base.inl"

#endif  // STARKWARE_ALGEBRA_FIELD_ELEMENT_BASE_H_
