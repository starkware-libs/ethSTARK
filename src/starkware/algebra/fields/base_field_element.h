#ifndef STARKWARE_ALGEBRA_FIELDS_BASE_FIELD_ELEMENT_H_
#define STARKWARE_ALGEBRA_FIELDS_BASE_FIELD_ELEMENT_H_

#include <string>

#include "starkware/algebra/field_element_base.h"
#include "starkware/math/math.h"
#include "starkware/randomness/prng.h"

namespace starkware {

/*
  This is an implementation of the field used in Rescue.
  The elements fit in one 64 bit word, and are stored in Montgomery representation for faster
  modular multiplication. For more information about Montgomery representation, and details about
  the implementation of operations (such as MontgomeryMul, Inverse, etc.), visit
  https://en.wikipedia.org/wiki/Montgomery_modular_multiplication .
*/
class BaseFieldElement : public FieldElementBase<BaseFieldElement> {
 public:
  static constexpr uint64_t kModulus = 0x2000001400000001;  // 2**61 + 20 * 2**32 + 1.
  static constexpr uint64_t kModulusBits = Log2Floor(kModulus);
  static constexpr uint64_t kMontgomeryR = 0x1fffff73fffffff9;  // = 2^64 % kModulus.
  static constexpr uint64_t kMontgomeryRSquared = 0x1fc18a13fffce041;
  static constexpr uint64_t kMontgomeryRCubed = 0x1dcf974ec7cafec4;
  static constexpr uint64_t kMontgomeryMPrime = 0x20000013ffffffff;  // = (-(kModulus^-1)) mod 2^64.

#ifdef NDEBUG
  // The default constructor is allowed to be used only in Release builds in order to reduce memory
  // allocation time for vectors of field elements.
  BaseFieldElement() = default;
#else
  // In debug builds, the default constructor is not allowed to be called at all.
  BaseFieldElement() = delete;
#endif

  static constexpr BaseFieldElement Zero() { return BaseFieldElement(0); }

  static constexpr BaseFieldElement One() { return BaseFieldElement(kMontgomeryR); }

  static BaseFieldElement Uninitialized() {
    // In the current implementation this function always returns zero.
    // If efficiency is necessary it can be changed to return an uninitialized value instead.
    return Zero();
  }

  static constexpr BaseFieldElement FromUint(uint64_t val) {
    // Note that because MontgomeryMul divides by r, we need to multiply by r^2 here.
    return BaseFieldElement(MontgomeryMul(val, kMontgomeryRSquared));
  }

  BaseFieldElement operator+(const BaseFieldElement& rhs) const {
    return BaseFieldElement(ReduceIfNeeded(value_ + rhs.value_));
  }

  BaseFieldElement operator-(const BaseFieldElement& rhs) const {
    uint64_t val = value_ - rhs.value_;
    return BaseFieldElement(IsNegative(val) ? val + kModulus : val);
  }

  BaseFieldElement operator-() const { return Zero() - *this; }

  BaseFieldElement operator*(const BaseFieldElement& rhs) const {
    return BaseFieldElement(MontgomeryMul(value_, rhs.value_));
  }

  constexpr bool operator==(const BaseFieldElement& rhs) const { return value_ == rhs.value_; }

  constexpr BaseFieldElement Inverse() const;

  /*
    Returns a byte serialization of the field element.
  */
  void ToBytes(gsl::span<std::byte> span_out) const;

  static BaseFieldElement RandomElement(Prng* prng);

  static BaseFieldElement FromBytes(gsl::span<const std::byte> bytes);

  static BaseFieldElement FromString(const std::string& s);

  std::string ToString() const;

  uint64_t ToStandardForm() const;

  /*
    Returns a generator of the multiplicative group of the field.
  */
  static constexpr BaseFieldElement Generator() { return BaseFieldElement::FromUint(3); }

  /*
    Returns the prime factors in the factorization of the size of the multiplicative group of the
    field (kModulus - 1).
  */
  static constexpr std::array<uint64_t, 5> PrimeFactors() { return {2, 13, 167, 211, 293}; }

  static constexpr uint64_t FieldSize() { return kModulus; }
  static constexpr size_t SizeInBytes() { return sizeof(uint64_t); }

 private:
  explicit constexpr BaseFieldElement(uint64_t val) : value_(val) {}

  static constexpr bool IsNegative(uint64_t val) { return static_cast<int64_t>(val) < 0; }

  /*
    In montgomery representation there might be an overflow of the value. i.e val > kModulus.
    This functions takes val, and returns an equivalent value such that 0 <= value < kModulus.
  */
  static constexpr uint64_t ReduceIfNeeded(uint64_t val) {
    uint64_t alt_val = val - kModulus;
    return IsNegative(alt_val) ? val : alt_val;
  }

  static constexpr __uint128_t Umul128(uint64_t x, uint64_t y) {
    return static_cast<__uint128_t>(x) * static_cast<__uint128_t>(y);
  }

  /*
    Computes (x*y / (2^64)) mod kModulus.
  */
  static constexpr uint64_t MontgomeryMul(uint64_t x, uint64_t y) {
    __uint128_t mul_res = Umul128(x, y);
    uint64_t u = static_cast<uint64_t>(mul_res) * kMontgomeryMPrime;
    __uint128_t res = Umul128(kModulus, u) + mul_res;

    ASSERT_DEBUG(static_cast<uint64_t>(res) == 0, "Low 64bit should be 0.");

    return ReduceIfNeeded(static_cast<uint64_t>(res >> 64));
  }

  uint64_t value_ = 0;
};

}  // namespace starkware

#include "src/starkware/algebra/fields/base_field_element.inl"

#endif  // STARKWARE_ALGEBRA_FIELDS_BASE_FIELD_ELEMENT_H_
