#ifndef STARKWARE_ALGEBRA_FIELD_OPERATIONS_H_
#define STARKWARE_ALGEBRA_FIELD_OPERATIONS_H_

#include <vector>

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

using std::size_t;
using std::uint64_t;

// --- Basic field-agnostic operations ---

/*
  Returns the power of a field element.
  Note that this function doesn't support negative exponents.
*/
template <typename FieldElementT>
FieldElementT Pow(const FieldElementT& base, __uint128_t exp) {
  FieldElementT power = base;
  FieldElementT res = FieldElementT::One();
  while (exp != 0) {
    if ((exp & 1) == 1) {
      res *= power;
    }
    power *= power;
    exp >>= 1;
  }
  return res;
}

// --- Getters ---

/*
  Returns a generator of a subgroup with size n.
  Requirements:
   1. n must be a power of 2.
   2. n must divide the value (FieldSize - 1).
*/
inline BaseFieldElement GetSubGroupGenerator(uint64_t n) {
  ASSERT_RELEASE(IsPowerOfTwo(n), "Subgroup size must be a power of 2.");
  const uint64_t quotient = SafeDiv(BaseFieldElement::FieldSize() - 1, n);
  return Pow(BaseFieldElement::Generator(), quotient);
}

/*
  Returns a random field element different from zero.
*/
template <typename FieldElementT>
FieldElementT RandomNonZeroElement(Prng* prng) {
  auto x = FieldElementT::RandomElement(prng);
  while (x == FieldElementT::Zero()) {
    x = FieldElementT::RandomElement(prng);
  }
  return x;
}

// --- Batch operations ---

/*
  Substitutes output value with the queried powers of a field element. For example, given as input
  base=x and exp={n1,n2,n3}, the output is {x^n1, x^n2, x^n3}.
  Note that this function doesn't support negative exponents.
*/
template <typename FieldElementT>
void BatchPow(
    const FieldElementT& base, gsl::span<const uint64_t> exponents,
    gsl::span<FieldElementT> output) {
  ASSERT_RELEASE(exponents.size() == output.size(), "Size mismatch");
  FieldElementT power = base;
  std::fill(output.begin(), output.end(), FieldElementT::One());

  // Find the number of bits to iterate on (maximal set bit index among exponents).
  const uint64_t exponents_or =
      std::accumulate(exponents.begin(), exponents.end(), 0, std::bit_or<>());
  const size_t n_bits = (exponents_or == 0 ? 0 : Log2Floor(exponents_or) + 1);

  // This implementation is a simple generalization of the modular exponentiation algorithm, using
  // the fact that the computed powers of the base are independent of the exponent itself, thus
  // applied on a batch can save on computation. We iterate over the bits of all exponents, from the
  // LSB to the highest bit set. In every index we advance the 'power' by multiplying it by itself,
  // and update output[i] only if the current iteration bit is set in the i'th exponent.
  for (size_t bit_idx = 0; bit_idx < n_bits; ++bit_idx) {
    const uint64_t test_mask = UINT64_C(1) << bit_idx;

    for (size_t i = 0; i < exponents.size(); ++i) {
      if ((exponents[i] & test_mask) != 0) {
        output[i] *= power;
      }
    }

    power *= power;
  }
}

/*
  Returns the queried powers of a field element, see previous BatchPow for more details. This
  BatchPow version allocates the memory for the output rather than getting it as a parameter.
*/
template <typename FieldElementT>
std::vector<FieldElementT> BatchPow(
    const FieldElementT& base, gsl::span<const uint64_t> exponents) {
  std::vector<FieldElementT> res = FieldElementT::UninitializedVector(exponents.size());
  BatchPow<FieldElementT>(base, exponents, res);
  return res;
}

/*
  A helper function for UninitializedFieldElementArray. Returns an array of uninitialized field
  elements in the same length as I.
*/
template <typename FieldElementT, size_t... I>
std::array<FieldElementT, sizeof...(I)> UninitializedFieldElementArrayImpl(
    std::index_sequence<I...> /*unused*/) {
  // We need to instantiate the array with sizeof...(I) copies of FieldElementT::Uninitialized().
  // To do it, we use the following trick:
  //
  // The expression (I, FieldElementT::Uninitialized()) simply evaluates to
  // FieldElementT::Uninitialized() (this is the comma operator in C++).
  // On the other hand, it depends on I, so adding the '...', evaluates it for every element of
  // I. Thus we obtain a list which is equivalent to:
  //   FieldElementT::Uninitialized(), FieldElementT::Uninitialized(), ...
  // where the number of instances is sizeof...(I).
  return {((void)I, FieldElementT::Uninitialized())...};
}

/*
  Returns an array of N uninitialized field elements.
*/
template <typename FieldElementT, size_t N>
std::array<FieldElementT, N> UninitializedFieldElementArray() {
  return UninitializedFieldElementArrayImpl<FieldElementT>(std::make_index_sequence<N>());
}

/*
  Returns the inner product of two given vectors.
*/
template <size_t N>
BaseFieldElement InnerProduct(
    const std::array<BaseFieldElement, N>& vector_a,
    const std::array<BaseFieldElement, N>& vector_b);

/*
  Performs a linear transformation (represented by a matrix) on a vector, and stores the result in
  output.
*/
template <size_t N>
void LinearTransformation(
    const std::array<std::array<BaseFieldElement, N>, N>& matrix,
    const std::array<BaseFieldElement, N>& vector, std::array<BaseFieldElement, N>* output);

}  // namespace starkware

#include "starkware/algebra/field_operations.inl"

#endif  // STARKWARE_ALGEBRA_FIELD_OPERATIONS_H_
