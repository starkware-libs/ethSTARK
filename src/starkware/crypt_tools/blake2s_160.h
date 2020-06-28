#ifndef STARKWARE_CRYPT_TOOLS_BLAKE2S_160_H_
#define STARKWARE_CRYPT_TOOLS_BLAKE2S_160_H_

#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

class Blake2s160 {
 public:
  static constexpr size_t kDigestNumBytes = 160 / 8;

  /*
    Supports the creation of uninitialized Blake2s160 vectors.
  */
  Blake2s160() {}  // NOLINT

  /*
    Gets a Blake2s160 instance with the specified digest.
  */
  static const Blake2s160 InitDigestTo(gsl::span<const std::byte> digest);

  static const Blake2s160 Hash(const Blake2s160& val1, const Blake2s160& val2);

  static const Blake2s160 HashBytesWithLength(gsl::span<const std::byte> bytes);

  bool operator==(const Blake2s160& other) const;
  bool operator!=(const Blake2s160& other) const;
  const std::array<std::byte, kDigestNumBytes>& GetDigest() const { return buffer_; }
  std::string ToString() const;
  friend std::ostream& operator<<(std::ostream& out, const Blake2s160& hash);

 private:
  static constexpr size_t kDigestNumDWords = kDigestNumBytes / sizeof(uint32_t);

  /*
    Constructs a Blake2s160 object based on the given data (assumed to be of size 20 bytes).
  */
  explicit Blake2s160(gsl::span<const std::byte> data);

  std::array<std::byte, kDigestNumBytes> buffer_;
};

}  // namespace starkware

#include "starkware/crypt_tools/blake2s_160.inl"

#endif  // STARKWARE_CRYPT_TOOLS_BLAKE2S_160_H_
