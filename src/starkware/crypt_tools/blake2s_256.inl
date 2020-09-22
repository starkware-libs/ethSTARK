#include "third_party/blake2/blake2-impl.h"
#include "third_party/blake2/blake2.h"

#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/to_from_string.h"

namespace starkware {

namespace blake2s_256 {
namespace details {

inline std::array<std::byte, Blake2s256::kDigestNumBytes> InitDigestFromSpan(
    gsl::span<const std::byte> data) {
  ASSERT_RELEASE(
      data.size() == Blake2s256::kDigestNumBytes, "Invalid digest initialization length.");
  std::array<std::byte, Blake2s256::kDigestNumBytes> digest;  // NOLINT
  std::copy(data.begin(), data.end(), digest.begin());
  return digest;
}

}  // namespace details
}  // namespace blake2s_256

inline Blake2s256::Blake2s256(gsl::span<const std::byte> data)
    : buffer_(blake2s_256::details::InitDigestFromSpan(data)) {}

inline const Blake2s256 Blake2s256::InitDigestTo(gsl::span<const std::byte> digest) {
  return Blake2s256(digest);
}

inline const Blake2s256 Blake2s256::Hash(const Blake2s256& val1, const Blake2s256& val2) {
  std::array<std::byte, 2 * kDigestNumBytes> data{};
  std::copy(val1.buffer_.begin(), val1.buffer_.end(), data.begin());
  std::copy(val2.buffer_.begin(), val2.buffer_.end(), data.begin() + kDigestNumBytes);
  return HashBytesWithLength(data);
}

inline const Blake2s256 Blake2s256::HashBytesWithLength(gsl::span<const std::byte> bytes) {
  Blake2s256 result;
  blake2s_state ctx;
  blake2s_init(&ctx, kDigestNumBytes);
  blake2s_update(&ctx, bytes.data(), bytes.size());
  blake2s_final(&ctx, gsl::make_span(result.buffer_).as_span<uint8_t>().data(), kDigestNumBytes);

  return result;
}

inline bool Blake2s256::operator==(const Blake2s256& other) const {
  return buffer_ == other.buffer_;
}
inline bool Blake2s256::operator!=(const Blake2s256& other) const { return !(*this == other); }

inline std::string Blake2s256::ToString() const {
  return BytesToHexString(buffer_, /*trim_leading_zeros=*/false);
}

inline std::ostream& operator<<(std::ostream& out, const Blake2s256& hash) {
  return out << hash.ToString();
}

}  // namespace starkware
