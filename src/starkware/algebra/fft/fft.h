#ifndef STARKWARE_ALGEBRA_FFT_FFT_H_
#define STARKWARE_ALGEBRA_FFT_FFT_H_

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/fields/base_field_element.h"

namespace starkware {

/*
  Computes the evaluations of a given polynomial.
  generator and offset define the coset on which the FFT is computed.
  If eval_in_natural_order is true then the output evaluations are in natural order and the input
  coefficients are in bit-reversed order. Otherwise, the evaluations are in bit-reversed order and
  the coefficients are in natural order.
*/
template <typename FieldElementT>
void Fft(
    gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst,
    const BaseFieldElement& generator, const BaseFieldElement& offset, bool eval_in_natural_order);

/*
  Computes the coefficients of the lowest degree polynomial whose evaluations are given in the
  input.
  generator and offset define the coset on which the IFFT is computed.
  The output is not normalized, i.e. the output is the coefficients of the polynomial times the
  number of evaluation points (==src.size()).
  If eval_in_natural_order is true then the input evaluations are in natural order and the output
  coefficients are in bit-reversed order. Otherwise, the evaluations are in bit-reversed order and
  the coefficients are in natural order.
*/
template <typename FieldElementT>
void Ifft(
    gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst,
    const BaseFieldElement& generator, const BaseFieldElement& offset, bool eval_in_natural_order);

/*
  Computes n_layers of IFFT where the input is in bit-reversed order and the output is in natural
  order.
*/
template <typename FieldElementT>
void IfftReverseToNatural(
    gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst,
    const BaseFieldElement& generator, const BaseFieldElement& offset, size_t n_layers);

}  // namespace starkware

#include "starkware/algebra/fft/fft.inl"

#endif  // STARKWARE_ALGEBRA_FFT_FFT_H_
