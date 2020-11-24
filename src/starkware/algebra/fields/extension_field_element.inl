#include "starkware/algebra/fields/base_field_element.h"

namespace starkware {

inline ExtensionFieldElement ExtensionFieldElement::operator+(
    const ExtensionFieldElement& rhs) const {
  return {coef0_ + rhs.coef0_, coef1_ + rhs.coef1_, coef2_ + rhs.coef2_};
}

inline ExtensionFieldElement ExtensionFieldElement::operator+(const BaseFieldElement& rhs) const {
  return {coef0_ + rhs, coef1_, coef2_};
}

template <typename FieldElementT>
ExtensionFieldElement ExtensionFieldElement::operator+=(const FieldElementT& rhs) {
  return *this = *this + rhs;
}

inline ExtensionFieldElement ExtensionFieldElement::operator-(
    const ExtensionFieldElement& rhs) const {
  return {coef0_ - rhs.coef0_, coef1_ - rhs.coef1_, coef2_ - rhs.coef2_};
}

inline ExtensionFieldElement ExtensionFieldElement::operator-(const BaseFieldElement& rhs) const {
  return {coef0_ - rhs, coef1_, coef2_};
}

ALWAYS_INLINE ExtensionFieldElement
    ExtensionFieldElement::operator*(const ExtensionFieldElement& rhs) const {
  const BaseFieldElement mul00 = coef0_ * rhs.coef0_;
  const BaseFieldElement mul11 = coef1_ * rhs.coef1_;
  const BaseFieldElement mul22 = coef2_ * rhs.coef2_;

  const BaseFieldElement three_times_mul22 = mul22 + mul22 + mul22;
  const BaseFieldElement mul11_minus_mul00 = mul11 - mul00;

  const BaseFieldElement mul00_plus_mul01_plus_mul10_plus_mul11 =
      (coef0_ + coef1_) * (rhs.coef0_ + rhs.coef1_);
  const BaseFieldElement mul00_plus_mul02_plus_mul20_plus_mul22 =
      (coef0_ + coef2_) * (rhs.coef0_ + rhs.coef2_);
  const BaseFieldElement mul11_plus_mul12_plus_mul21_plus_mul22 =
      (coef1_ + coef2_) * (rhs.coef1_ + rhs.coef2_);

  return {
      mul11_plus_mul12_plus_mul21_plus_mul22 - (mul22 + mul11_minus_mul00),
      mul00_plus_mul01_plus_mul10_plus_mul11 + mul11_minus_mul00 + three_times_mul22 -
          (mul11_plus_mul12_plus_mul21_plus_mul22 + mul11_plus_mul12_plus_mul21_plus_mul22),
      mul00_plus_mul02_plus_mul20_plus_mul22 + mul11_minus_mul00 - three_times_mul22,
  };
}

ALWAYS_INLINE ExtensionFieldElement
    ExtensionFieldElement::operator*(const BaseFieldElement& rhs) const {
  return {coef0_ * rhs, coef1_ * rhs, coef2_ * rhs};
}

template <typename FieldElementT>
ALWAYS_INLINE ExtensionFieldElement& ExtensionFieldElement::operator*=(const FieldElementT& rhs) {
  return *this = *this * rhs;
}

inline ExtensionFieldElement ExtensionFieldElement::operator/(const BaseFieldElement& rhs) const {
  return *this * rhs.Inverse();
}

inline ExtensionFieldElement ExtensionFieldElement::Inverse() const {
  // To compute the inverse 1/z, we multiply both numerator and denominator by the two conjugates of
  // z (z' and z''), and get: (z'*z'')/norm(z). Inversing the norm of z is done in the base field.
  ASSERT_RELEASE(*this != Zero(), "Zero does not have an inverse");
  const ExtensionFieldElement conj1 = GetFrobenius();
  const ExtensionFieldElement conj2 = conj1.GetFrobenius();
  const ExtensionFieldElement numerator = conj1 * conj2;
  const ExtensionFieldElement norm = *this * numerator;
  ASSERT_RELEASE(norm.IsInBaseField(), "Expecting the norm to be a base field element.");
  const BaseFieldElement denominator = norm.coef0_;
  return numerator * denominator.Inverse();
}

inline void ExtensionFieldElement::ToBytes(gsl::span<std::byte> span_out) const {
  coef0_.ToBytes(span_out.subspan(0, BaseFieldElement::SizeInBytes()));
  coef1_.ToBytes(
      span_out.subspan(BaseFieldElement::SizeInBytes(), BaseFieldElement::SizeInBytes()));
  coef2_.ToBytes(
      span_out.subspan(2 * BaseFieldElement::SizeInBytes(), BaseFieldElement::SizeInBytes()));
}

inline ExtensionFieldElement ExtensionFieldElement::FromBytes(gsl::span<const std::byte> bytes) {
  return ExtensionFieldElement(
      BaseFieldElement::FromBytes(bytes.subspan(0, BaseFieldElement::SizeInBytes())),
      BaseFieldElement::FromBytes(
          bytes.subspan(BaseFieldElement::SizeInBytes(), BaseFieldElement::SizeInBytes())),
      BaseFieldElement::FromBytes(
          bytes.subspan(2 * BaseFieldElement::SizeInBytes(), BaseFieldElement::SizeInBytes())));
}

inline std::string ExtensionFieldElement::ToString() const {
  std::string str0 = coef0_.BaseFieldElement::ToString();
  std::string str1 = coef1_.BaseFieldElement::ToString();
  std::string str2 = coef2_.BaseFieldElement::ToString();
  return str0 + "::" + str1 + "::" + str2;
}

inline ExtensionFieldElement ExtensionFieldElement::FromString(const std::string& s) {
  size_t first_split_point = s.find("::");
  // When converting a base field element string to an extension field, coef1 and coef2 might not be
  // mentioned in the string, so the entire string is coef0.
  if (first_split_point == std::string::npos) {
    return ExtensionFieldElement(BaseFieldElement::FromString(s));
  }
  size_t second_split_point = s.find("::", first_split_point + 2);
  ASSERT_RELEASE(second_split_point != std::string::npos, "Bad ExtensionFieldElement format: " + s);
  return ExtensionFieldElement(
      BaseFieldElement::FromString(s.substr(0, first_split_point)),
      BaseFieldElement::FromString(
          s.substr(first_split_point + 2, second_split_point - first_split_point - 2)),
      BaseFieldElement::FromString(
          s.substr(second_split_point + 2, s.length() - second_split_point - 2)));
}

inline ExtensionFieldElement operator-(
    const BaseFieldElement& lhs, const ExtensionFieldElement& rhs) {
  return ExtensionFieldElement(lhs - rhs.coef0_, -rhs.coef1_, -rhs.coef2_);
}

ALWAYS_INLINE ExtensionFieldElement
operator*(const BaseFieldElement& lhs, const ExtensionFieldElement& rhs) {
  return rhs * lhs;
}

}  // namespace starkware
