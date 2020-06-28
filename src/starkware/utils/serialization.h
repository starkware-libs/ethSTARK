#ifndef STARKWARE_UTILS_SERIALIZATION_H_
#define STARKWARE_UTILS_SERIALIZATION_H_

#include <endian.h>
#include <algorithm>
#include <cstddef>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/error_handling/error_handling.h"

namespace starkware {

inline void Serialize(uint64_t val, gsl::span<std::byte> span_out) {
  ASSERT_RELEASE(
      span_out.size() == sizeof(uint64_t), "Destination span size mismatches uint64_t size.");
  val = htobe64(val);                                           // NOLINT
  const auto bytes = gsl::byte_span(val).as_span<std::byte>();  // NOLINT
  std::copy(bytes.begin(), bytes.end(), span_out.begin());
}

inline uint64_t Deserialize(gsl::span<const std::byte> span) {
  ASSERT_RELEASE(span.size() == sizeof(uint64_t), "Source span size mismatches uint64_t size.");
  uint64_t val;
  const auto bytes = gsl::byte_span(val).as_span<std::byte>();  // NOLINT
  // Use span.begin() + sizeof(uint64_t) rather than span.end() to allow the compiler to
  // deduce the size as a compile time const.
  std::copy(span.begin(), span.begin() + sizeof(uint64_t), bytes.begin());
  return be64toh(val);  // NOLINT
}

}  // namespace starkware

#endif  // STARKWARE_UTILS_SERIALIZATION_H_
