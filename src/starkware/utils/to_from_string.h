#ifndef STARKWARE_UTILS_TO_FROM_STRING_H_
#define STARKWARE_UTILS_TO_FROM_STRING_H_

#include <cstddef>
#include <iomanip>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

/*
  Converts an array of bytes to an ASCII string with the option to trim leading zeros.
  Example: [00, 2B, AA, 10] -> "0x2BAA10".
*/
std::string BytesToHexString(gsl::span<const std::byte> data, bool trim_leading_zeros = true);

/*
  Converts an ASCII string to an array of bytes.
  If the input string isn't long enough to fill the entire byte array then the remainder is padded
  with leading zeroes.
  Example: "0x2BAA10" -> [00, 2B, AA, 10] (assuming as_bytes_out.size() == 4).
*/
void HexStringToBytes(const std::string& hex_string, gsl::span<std::byte> as_bytes_out);

}  // namespace starkware

#endif  // STARKWARE_UTILS_TO_FROM_STRING_H_
