#include "starkware/utils/to_from_string.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

#include "starkware/error_handling/error_handling.h"

namespace starkware {

std::string BytesToHexString(gsl::span<const std::byte> data, bool trim_leading_zeros) {
  ASSERT_RELEASE(!data.empty(), "Cannot convert empty byte sequence to hex.");
  std::stringstream s;
  auto iter = data.begin();
  s << "0x";
  if (trim_leading_zeros) {
    // Skip leading zeroes so that they are not inserted into the string.
    for (; iter != data.end() && std::to_integer<int>(*iter) == 0; ++iter) {
    }
    // If all bytes are zeroes, output the hex equivalent for zero.
    if (iter == data.end()) {
      return "0x0";
    }
    // Note that each byte should be represented by two consecutive string characters, so bytes that
    // are too small are paired with a '0' character to pad them appropriately in the corresponding
    // output string.
    // In the case where we want to trim leading zeroes, this is done for every byte except for the
    // first (most significant) one, which is inserted here.
    s << std::hex << std::to_integer<int>(*iter);
    ++iter;
  }

  for (; iter != data.end(); ++iter) {
    // Insert bytes as hex characters, padding with a '0' character where necessary.
    s << std::setfill('0') << std::setw(2) << std::hex << std::to_integer<int>(*iter);
  }

  return s.str();
}

void HexStringToBytes(const std::string& hex_string, gsl::span<std::byte> as_bytes_out) {
  ASSERT_RELEASE(
      hex_string.length() > 2,
      "String (\"" + hex_string +
          "\") is too short, expected at least two chars (for the '0x' prefix).");
  const std::string hex_prefix = "0x";
  auto iter_pair =
      std::mismatch(hex_prefix.begin(), hex_prefix.end(), hex_string.begin(), hex_string.end());
  ASSERT_RELEASE(
      hex_prefix.end() == iter_pair.first,
      "String (\"" + hex_string + "\") does not start with '0x'.");
  std::string pure_hex_representation = hex_string.substr(2);
  // Trim all leading zeroes.
  pure_hex_representation.erase(
      0,
      std::min(pure_hex_representation.find_first_not_of('0'), pure_hex_representation.size() - 1));
  // If the string is of odd length, we insert a single '0' character to its beginning in order to
  // simplify parsing (that way 1byte = two characters in the string).
  if (pure_hex_representation.length() % 2 != 0) {
    pure_hex_representation.insert(0, 1, '0');
  }

  // Not counting the '0x' prefix, the output's size should be at least half the hex-string's size,
  // since every consecutive pair of characters was converted to a corresponding byte.
  ASSERT_RELEASE(
      as_bytes_out.size() >= pure_hex_representation.length() / 2,
      "Output's length (" + std::to_string(as_bytes_out.size()) +
          ") is less than half of the pure hex number's length (" +
          std::to_string(pure_hex_representation.length() / 2) + ").");
  // If the byte span is longer than half the string's length, we calculate the amount of spare
  // bytes in order to pad them with leading zeroes.
  size_t offset = as_bytes_out.size() - pure_hex_representation.length() / 2;
  // Pad the unaccounted leading bytes with zeroes.
  std::fill(as_bytes_out.begin(), as_bytes_out.begin() + offset, std::byte{0});
  // This loop converts consecutive pairs of characters to corresponding bytes.
  for (size_t i = 0; i < pure_hex_representation.length() / 2; ++i) {
    std::stringstream str;
    str << std::hex << pure_hex_representation.substr(i * 2, 2);
    int c;
    str >> c;
    as_bytes_out[offset + i] = std::byte(c);
  }
}

}  // namespace starkware
