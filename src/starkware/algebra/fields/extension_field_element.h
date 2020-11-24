#ifndef STARKWARE_ALGEBRA_FIELDS_EXTENSION_FIELD_ELEMENT_H_
#define STARKWARE_ALGEBRA_FIELDS_EXTENSION_FIELD_ELEMENT_H_

#include <string>
#include <vector>

#include "starkware/algebra/field_element_base.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/base_field_element.h"

namespace starkware {

/*
  Represents an extension field element of the base field F (F's elements are of type
  BaseFieldElement). An element of the extension field is coef0_+coef1_*phi+coef2_*phi^2 where phi
  is a root of the polynomial X^3+2*X-1, and coef0_, coef1_, coef2_ are of type BaseFieldElement. In
  other words, the extension field is F[X]/(X^3+2*X-1).
*/
class ExtensionFieldElement : public FieldElementBase<ExtensionFieldElement> {
 public:
#ifdef NDEBUG
  // The default constructor is allowed to be used only in Release builds in order to reduce memory
  // allocation time for vectors of field elements.
  ExtensionFieldElement() = default;
#else
  // In debug builds, the default constructor is not allowed to be called at all.
  ExtensionFieldElement() = delete;
#endif

  static constexpr ExtensionFieldElement Zero() {
    return ExtensionFieldElement(BaseFieldElement::Zero());
  }

  static constexpr ExtensionFieldElement One() {
    return ExtensionFieldElement(BaseFieldElement::One());
  }

  static ExtensionFieldElement Uninitialized() {
    // In the current implementation this function always returns zero.
    // If efficiency is necessary it can be changed to return an uninitialized value instead.
    return Zero();
  }

  /*
    Creates an extension field element from three base field elements.
  */
  constexpr ExtensionFieldElement(
      const BaseFieldElement& coef0, const BaseFieldElement& coef1, const BaseFieldElement& coef2)
      : coef0_(coef0), coef1_(coef1), coef2_(coef2) {}

  /*
    Given a base field element coef0, creates the extension field element (coef0, 0, 0).
  */
  explicit constexpr ExtensionFieldElement(const BaseFieldElement& coef0)
      : coef0_(coef0), coef1_(BaseFieldElement::Zero()), coef2_(BaseFieldElement::Zero()) {}

  ExtensionFieldElement operator+(const ExtensionFieldElement& rhs) const;
  ExtensionFieldElement operator+(const BaseFieldElement& rhs) const;
  template <typename FieldElementT>
  ExtensionFieldElement operator+=(const FieldElementT& rhs);

  ExtensionFieldElement operator-(const ExtensionFieldElement& rhs) const;
  ExtensionFieldElement operator-(const BaseFieldElement& rhs) const;
  friend ExtensionFieldElement operator-(
      const BaseFieldElement& lhs, const ExtensionFieldElement& rhs);

  ExtensionFieldElement operator-() const {
    return ExtensionFieldElement(-coef0_, -coef1_, -coef2_);
  }

  ExtensionFieldElement operator*(const ExtensionFieldElement& rhs) const;
  ExtensionFieldElement operator*(const BaseFieldElement& rhs) const;
  template <typename FieldElementT>
  ExtensionFieldElement& operator*=(const FieldElementT& rhs);
  friend ExtensionFieldElement operator*(
      const BaseFieldElement& lhs, const ExtensionFieldElement& rhs);

  using FieldElementBase<ExtensionFieldElement>::operator/;
  ExtensionFieldElement operator/(const BaseFieldElement& rhs) const;

  constexpr bool operator==(const ExtensionFieldElement& rhs) const {
    return coef0_ == rhs.coef0_ && coef1_ == rhs.coef1_ && coef2_ == rhs.coef2_;
  }

  ExtensionFieldElement Inverse() const;

  ExtensionFieldElement GetFrobenius() const {
    return {
        coef0_ + BaseFieldElement::FromUint(318233216319004744) * coef1_ +
            BaseFieldElement::FromUint(2067168182873786313) * coef2_,
        BaseFieldElement::FromUint(179006184179440168) * coef1_ +
            BaseFieldElement::FromUint(159116608159502372) * coef2_,
        BaseFieldElement::FromUint(238674912239253558) * coef1_ +
            BaseFieldElement::FromUint(2126836910933599704) * coef2_,
    };
  }

  /*
    Returns a random extension field element: its coefficients are random BaseFieldElement generated
    by BaseFieldElement::RandomElement.
  */
  static ExtensionFieldElement RandomElement(Prng* prng) {
    return ExtensionFieldElement(
        BaseFieldElement::RandomElement(prng), BaseFieldElement::RandomElement(prng),
        BaseFieldElement::RandomElement(prng));
  }

  bool IsInBaseField() const {
    return coef1_ == BaseFieldElement::Zero() && coef2_ == BaseFieldElement::Zero();
  }

  /*
    Converts this extension field element to bytes using ToBytes of BaseFieldElement. The result is
    the concatenation of coef0_.ToBytes(), coef1_.ToBytes() and coef2_.ToBytes().
  */
  void ToBytes(gsl::span<std::byte> span_out) const;

  static ExtensionFieldElement FromBytes(gsl::span<const std::byte> bytes);

  std::string ToString() const;

  static ExtensionFieldElement FromString(const std::string& s);

  static constexpr ExtensionFieldElement FromUint(uint64_t val) {
    return ExtensionFieldElement(
        BaseFieldElement::FromUint(val), BaseFieldElement::Zero(), BaseFieldElement::Zero());
  }

  static constexpr size_t SizeInBytes() { return BaseFieldElement::SizeInBytes() * 3; }

 private:
  BaseFieldElement coef0_;
  BaseFieldElement coef1_;
  BaseFieldElement coef2_;
};

}  // namespace starkware

#include "starkware/algebra/fields/extension_field_element.inl"

#endif  // STARKWARE_ALGEBRA_FIELDS_EXTENSION_FIELD_ELEMENT_H_
