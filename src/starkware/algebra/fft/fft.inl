#include "gflags/gflags.h"

#include "starkware/algebra/field_operations.h"
#include "starkware/math/math.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {

template <typename FieldElementT>
void IfftReverseToNatural(
    gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst,
    const BaseFieldElement& generator, const BaseFieldElement& offset, size_t n_layers) {
  ASSERT_RELEASE(
      n_layers > 0, "n_layers (" + std::to_string(n_layers) + ") must be greater then 0.");
  BaseFieldElement layer_offset_inverse = offset.Inverse();
  BaseFieldElement layer_generator_inverse = generator.Inverse();
  const size_t n = src.size();
  size_t distance = 1;
  gsl::span<const FieldElementT> curr_src = src;
  for (size_t layer_i = 0; layer_i < n_layers; ++layer_i) {
    for (size_t i = 0; i < n; i += 2 * distance) {
      BaseFieldElement x_inverse = layer_offset_inverse;
      x_inverse *= Pow(
          layer_generator_inverse, BitReverse(SafeDiv(i, 2 * distance), SafeLog2(n) - 1 - layer_i));
      for (size_t j = 0, idx = i; j < distance; ++j, ++idx) {
        // Note that starting from the second iteration, src == dst so we must use temporary
        // variables.
        const FieldElementT left = curr_src[idx];
        const FieldElementT right = curr_src[idx + distance];
        dst[idx] = left + right;
        dst[idx + distance] = x_inverse * (left - right);
      }
    }
    // First IFFT iteration copies the data, the following iterations work in-place.
    curr_src = dst;
    distance <<= 1;
    layer_offset_inverse *= layer_offset_inverse;
    layer_generator_inverse *= layer_generator_inverse;
  }
}

template <typename FieldElementT>
void IfftNaturalToReverse(
    gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst,
    const BaseFieldElement& generator, const BaseFieldElement& offset) {
  BaseFieldElement layer_offset_inverse = offset.Inverse();
  BaseFieldElement layer_generator_inverse = generator.Inverse();
  const size_t n = src.size();
  const size_t n_layers = SafeLog2(n);
  size_t distance = n;
  gsl::span<const FieldElementT> curr_src = src;
  for (size_t layer_i = 0; layer_i < n_layers; ++layer_i) {
    distance >>= 1;
    for (size_t i = 0; i < n; i += 2 * distance) {
      BaseFieldElement x_inverse = layer_offset_inverse;
      for (size_t j = 0, idx = i; j < distance; ++j, ++idx) {
        // Note that starting from the second iteration, src == dst so we must use temporary
        // variables.
        const FieldElementT left = curr_src[idx];
        const FieldElementT right = curr_src[idx + distance];
        dst[idx] = left + right;
        dst[idx + distance] = x_inverse * (left - right);
        x_inverse *= layer_generator_inverse;
      }
    }
    // First ifft iteration copies the data, the following iterations work in-place.
    curr_src = dst;
    layer_offset_inverse *= layer_offset_inverse;
    layer_generator_inverse *= layer_generator_inverse;
  }
}

template <typename FieldElementT>
void FftReverseToNatural(
    gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst,
    const BaseFieldElement& generator, const BaseFieldElement& offset) {
  const size_t n = src.size();
  const size_t n_layers = SafeLog2(n);
  size_t distance = 1;
  gsl::span<const FieldElementT> curr_src = src;
  for (size_t layer_i = 0; layer_i < n_layers; ++layer_i) {
    const BaseFieldElement layer_generator = Pow(generator, Pow2(n_layers - 1 - layer_i));
    const BaseFieldElement layer_offset = Pow(offset, Pow2(n_layers - 1 - layer_i));
    for (size_t i = 0; i < n; i += 2 * distance) {
      BaseFieldElement x = layer_offset;
      for (size_t j = 0, idx = i; j < distance; ++j, ++idx) {
        // Note that starting from the second iteration, src == dst so we must use temporary
        // variables.
        const FieldElementT left = curr_src[idx];
        const FieldElementT x_times_right = x * curr_src[idx + distance];
        dst[idx] = left + x_times_right;
        dst[idx + distance] = left - x_times_right;
        x *= layer_generator;
      }
    }
    // First fft iteration copies the data, the following iterations work in-place.
    curr_src = dst;
    distance <<= 1;
  }
}

template <typename FieldElementT>
void FftNaturalToReverse(
    gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst,
    const BaseFieldElement& generator, const BaseFieldElement& offset) {
  const size_t n = src.size();
  const size_t n_layers = SafeLog2(n);
  size_t distance = n;
  gsl::span<const FieldElementT> curr_src = src;
  for (size_t layer_i = 0; layer_i < n_layers; ++layer_i) {
    distance >>= 1;
    const BaseFieldElement layer_offset = Pow(offset, Pow2(n_layers - 1 - layer_i));
    for (size_t i = 0; i < n; i += 2 * distance) {
      BaseFieldElement x = layer_offset;
      x *= Pow(generator, BitReverse(SafeDiv(i, 2 * distance), n_layers - 1));
      for (size_t j = 0, idx = i; j < distance; ++j, ++idx) {
        // Note that starting from the second iteration, src == dst so we must use temporary
        // variables.
        const FieldElementT left = curr_src[idx];
        const FieldElementT x_times_right = x * curr_src[idx + distance];
        dst[idx] = left + x_times_right;
        dst[idx + distance] = left - x_times_right;
      }
    }
    // First FFT iteration copies the data, the following iterations work in-place.
    curr_src = dst;
  }
}

template <typename FieldElementT>
void Fft(
    gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst,
    const BaseFieldElement& generator, const BaseFieldElement& offset, bool eval_in_natural_order) {
  if (eval_in_natural_order) {
    FftReverseToNatural<FieldElementT>(src, dst, generator, offset);
  } else {
    FftNaturalToReverse<FieldElementT>(src, dst, generator, offset);
  }
}

template <typename FieldElementT>
void Ifft(
    gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst,
    const BaseFieldElement& generator, const BaseFieldElement& offset, bool eval_in_natural_order) {
  if (src.size() == 1) {
    dst[0] = src[0];
    return;
  }
  if (eval_in_natural_order) {
    IfftNaturalToReverse<FieldElementT>(src, dst, generator, offset);
  } else {
    IfftReverseToNatural<FieldElementT>(src, dst, generator, offset, SafeLog2(src.size()));
  }
}

}  // namespace starkware
