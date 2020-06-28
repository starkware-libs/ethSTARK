#ifndef STARKWARE_UTILS_BIT_REVERSAL_H_
#define STARKWARE_UTILS_BIT_REVERSAL_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"

namespace starkware {

using std::size_t;

/*
  Returns the bit-reversal of n assuming it has the given number of bits.
  For example:
    If we have number_of_bits = 6 and n = (0b)1101 == (0b)001101
    the function will return (0b)101100.
*/
static inline uint64_t BitReverse(uint64_t n, const size_t number_of_bits) {
  if (number_of_bits == 0) {
    // Avoid undefined behavior when shifting by 64 - number_of_bits.
    return n;
  }

  ASSERT_RELEASE(
      number_of_bits == 64 || n < Pow2(number_of_bits), "n must be smaller than 2^number_of_bits.");

  // Bit-reverse all the bytes.
  std::array<uint64_t, 3> masks{0x5555555555555555, 0x3333333333333333, 0xf0f0f0f0f0f0f0f};
  size_t bits_to_swap = 1;
  for (auto mask : masks) {
    n = ((n & mask) << bits_to_swap) | ((n >> bits_to_swap) & mask);
    bits_to_swap <<= 1;
  }

  // Swap all the bytes using Built-in Functions, should result in 1 asm instruction.
  n = __builtin_bswap64(n);

  // Adjust the result by right-shifting the bits number_of_bits times.
  return n >> (64 - number_of_bits);
}

/*
  Applies the bit-reversal permutation on src span.
  The size of src needs to be of the form Pow2(k).
  The result should satisfy:
    dst[i] = src[BitReverse(i, k)] for each i in [0, Pow2(k)).
*/
template <typename FieldElementT>
void BitReverseVector(gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst);

/*
  Same as BitReverseVector(), but returns a result vector as a return value.
*/
template <typename FieldElementT>
std::vector<FieldElementT> BitReverseVector(gsl::span<const FieldElementT> src) {
  auto dst = FieldElementT::UninitializedVector(src.size());
  BitReverseVector(src, gsl::make_span(dst));
  return dst;
}

}  // namespace starkware

#include "starkware/utils/bit_reversal.inl"

#endif  // STARKWARE_UTILS_BIT_REVERSAL_H_
