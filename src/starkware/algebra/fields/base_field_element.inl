namespace starkware {

constexpr BaseFieldElement BaseFieldElement::Inverse() const {
  ASSERT_RELEASE(*this != BaseFieldElement::Zero(), "Zero does not have an inverse.");
  struct {
    uint64_t val = 0;
    uint64_t coef = 0;
  } u, v;

  u.val = value_;
  u.coef = 1;

  v.val = kModulus;
  v.coef = 0;

  while (1 < v.val) {
    if (u.val >= v.val) {
      const auto tmp = u;
      u = v;
      v = tmp;
    }

    uint64_t shifted_coef = u.coef;
    uint64_t shifted_val = u.val;
    while (true) {
      // The loop maintains the invariant that v.val = v.coeff * value (mod modulus).
      uint64_t tmp = shifted_val + shifted_val;
      bool carry = (shifted_val & (1ULL << 63)) != 0u;
      if (carry || tmp >= v.val) {
        break;
      }
      shifted_val = tmp;
      uint64_t res = shifted_coef + shifted_coef;
      carry = (shifted_coef & (1ULL << 63)) != 0u;
      shifted_coef = res;
      if (carry || shifted_coef >= kModulus) {
        shifted_coef = shifted_coef - kModulus;
      }
    }
    v.val = v.val - shifted_val;
    uint64_t res = v.coef - shifted_coef;
    bool carry = v.coef < res;
    v.coef = res;
    if (carry) {
      v.coef = v.coef + kModulus;
    }
  }
  ASSERT_RELEASE(
      v.val == 1, "GCD(value,modulus) is not 1, in particular, the value is not invertable.");

  // Currently, v.coef == ToStandardForm()^(-1) * kMontgomeryR^(-1).
  // The return value should be ToStandardForm()^(-1) * kMontgomeryR. Hence v.coef has to be
  // multiplied by kMontgomeryR^2, but MontgomeryMul also divides by kMontgomeryR, and so we
  // multiply by kMontgomeryR^3.
  return BaseFieldElement(MontgomeryMul(v.coef, kMontgomeryRCubed));
}

}  // namespace starkware
