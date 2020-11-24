#ifndef STARKWARE_MATH_MATH_H_
#define STARKWARE_MATH_MATH_H_

#include <cstdint>
#include <vector>

#include "starkware/error_handling/error_handling.h"

namespace starkware {

using std::size_t;

/*
  Returns 2^n, n must be < 64.
*/
constexpr uint64_t inline Pow2(uint64_t n) {
  ASSERT_RELEASE(n < 64, "n must be smaller than 64.");
  return UINT64_C(1) << n;
}

/*
  Checks if n is a power of 2.
*/
constexpr bool inline IsPowerOfTwo(const uint64_t n) { return (n != 0) && ((n & (n - 1)) == 0); }

/*
  Returns floor(Log_2(n)), n must be > 0.
*/
constexpr size_t inline Log2Floor(const uint64_t n) {
  ASSERT_RELEASE(n != 0, "log2 of 0 is undefined.");
  static_assert(
      sizeof(long long) == 8,  // NOLINT: use C type long long.
      "It is assumed that the type long long is represented by 64 bits.");
  return 63 - __builtin_clzll(n);
}

/*
  Returns ceil(Log_2(n)), n must be > 0.
*/
constexpr size_t inline Log2Ceil(const uint64_t n) {
  ASSERT_RELEASE(n != 0, "log2 of 0 is undefined.");
  return Log2Floor(n) + (IsPowerOfTwo(n) ? 0 : 1);
}

/*
  Computes log2(n) where n is a power of 2. This function fails if n is not a power of 2.
*/
size_t inline SafeLog2(const uint64_t n) {
  ASSERT_RELEASE(IsPowerOfTwo(n), "n must be a power of 2. n=" + std::to_string(n) + ".");
  return Log2Floor(n);
}

/*
  Computes x / y . This function fails if x % y != 0.
*/
constexpr uint64_t SafeDiv(const uint64_t numerator, const uint64_t denominator) {
  ASSERT_RELEASE(denominator != 0, "The denominator cannot be zero.");
  ASSERT_RELEASE(
      numerator % denominator == 0, "The denominator " + std::to_string(denominator) +
                                        " divides the numerator " + std::to_string(numerator) +
                                        " with a remainder.");
  return numerator / denominator;
}

/*
  Computes ceil(x / y).
*/
constexpr uint64_t DivCeil(const uint64_t numerator, const uint64_t denominator) {
  ASSERT_RELEASE(denominator != 0, "The denominator cannot be zero.");
  ASSERT_RELEASE(numerator + denominator > numerator, "Integer overflow.");
  return (numerator + denominator - 1) / denominator;
}

}  // namespace starkware

#endif  // STARKWARE_MATH_MATH_H_
