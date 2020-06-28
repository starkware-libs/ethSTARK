#include "starkware/algebra/fields/base_field_element.h"

namespace starkware {

inline ExtensionFieldElement ExtensionFieldElement::operator+(
    const ExtensionFieldElement& rhs) const {
  return {coef0_ + rhs.coef0_, coef1_ + rhs.coef1_};
}

inline ExtensionFieldElement ExtensionFieldElement::operator+(const BaseFieldElement& rhs) const {
  return {coef0_ + rhs, coef1_};
}

inline ExtensionFieldElement ExtensionFieldElement::operator-(
    const ExtensionFieldElement& rhs) const {
  return {coef0_ - rhs.coef0_, coef1_ - rhs.coef1_};
}

inline ExtensionFieldElement ExtensionFieldElement::operator-(const BaseFieldElement& rhs) const {
  return {coef0_ - rhs, coef1_};
}

ALWAYS_INLINE ExtensionFieldElement
    ExtensionFieldElement::operator*(const ExtensionFieldElement& rhs) const {
  BaseFieldElement coef0_mul = coef0_ * rhs.coef0_;
  return {coef0_mul + coef1_ * rhs.coef1_,
          (coef0_ + coef1_) * (rhs.coef0_ + rhs.coef1_) - coef0_mul};
}

ALWAYS_INLINE ExtensionFieldElement
    ExtensionFieldElement::operator*(const BaseFieldElement& rhs) const {
  return {coef0_ * rhs, coef1_ * rhs};
}

template <typename FieldElementT>
ALWAYS_INLINE ExtensionFieldElement& ExtensionFieldElement::operator*=(const FieldElementT& rhs) {
  return *this = *this * rhs;
}

inline ExtensionFieldElement ExtensionFieldElement::operator/(const BaseFieldElement& rhs) const {
  return *this * rhs.Inverse();
}

inline ExtensionFieldElement ExtensionFieldElement::Inverse() const {
  // The minimal polynomial of phi is x^2 - x - 1 whose roots are phi and 1-phi.
  // Thus, the conjugate of phi is 1-phi, and to compute the inverse we multiply the numerator and
  // denominator by (coef0_ + coef1_ - coef1_ * phi).
  ASSERT_RELEASE(*this != Zero(), "Zero does not have an inverse");
  const auto denom = coef0_ * coef0_ + coef0_ * coef1_ - coef1_ * coef1_;
  const auto denom_inv = denom.Inverse();
  return {(coef0_ + coef1_) * denom_inv, -coef1_ * denom_inv};
}

inline void ExtensionFieldElement::ToBytes(gsl::span<std::byte> span_out) const {
  coef0_.ToBytes(span_out.subspan(0, BaseFieldElement::SizeInBytes()));
  coef1_.ToBytes(
      span_out.subspan(BaseFieldElement::SizeInBytes(), BaseFieldElement::SizeInBytes()));
}

inline ExtensionFieldElement ExtensionFieldElement::FromBytes(gsl::span<const std::byte> bytes) {
  return ExtensionFieldElement(
      BaseFieldElement::FromBytes(bytes.subspan(0, BaseFieldElement::SizeInBytes())),
      BaseFieldElement::FromBytes(
          bytes.subspan(BaseFieldElement::SizeInBytes(), BaseFieldElement::SizeInBytes())));
}

inline std::string ExtensionFieldElement::ToString() const {
  std::string str0 = coef0_.BaseFieldElement::ToString();
  std::string str1 = coef1_.BaseFieldElement::ToString();
  return str0 + "::" + str1;
}

inline ExtensionFieldElement ExtensionFieldElement::FromString(const std::string& s) {
  size_t split_point = s.find("::");
  // When converting a base field element string to an extension field, coef1 might not be mentioned
  // in the string, so the entire string is coef0.
  if (split_point == std::string::npos) {
    return ExtensionFieldElement(BaseFieldElement::FromString(s), BaseFieldElement::Zero());
  }
  return ExtensionFieldElement(
      BaseFieldElement::FromString(s.substr(0, split_point)),
      BaseFieldElement::FromString(s.substr(split_point + 2, s.length())));
}

inline ExtensionFieldElement operator-(
    const BaseFieldElement& lhs, const ExtensionFieldElement& rhs) {
  return ExtensionFieldElement(lhs - rhs.coef0_, -rhs.coef1_);
}

ALWAYS_INLINE ExtensionFieldElement
operator*(const BaseFieldElement& lhs, const ExtensionFieldElement& rhs) {
  return rhs * lhs;
}

}  // namespace starkware
