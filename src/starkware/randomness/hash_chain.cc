#include "starkware/randomness/hash_chain.h"

#include <algorithm>
#include <array>

#include "glog/logging.h"

#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/utils/serialization.h"

namespace starkware {

HashChain::HashChain() {
  hash_ = Blake2s256::InitDigestTo(std::array<std::byte, Blake2s256::kDigestNumBytes>{});
}

HashChain::HashChain(gsl::span<const std::byte> public_input_data) {
  InitHashChain(public_input_data);
}

void HashChain::InitHashChain(gsl::span<const std::byte> bytes) {
  hash_ = Blake2s256::HashBytesWithLength(bytes);
  num_spare_bytes_ = 0;
  counter_ = 0;
}

void HashChain::GetRandomBytes(gsl::span<std::byte> random_bytes_out) {
  size_t num_bytes = random_bytes_out.size();
  size_t num_full_blocks = num_bytes / Blake2s256::kDigestNumBytes;

  for (size_t offset = 0; offset < num_full_blocks * Blake2s256::kDigestNumBytes;
       offset += Blake2s256::kDigestNumBytes) {
    GetMoreRandomBytesUsingHashWithCounter(
        counter_++, random_bytes_out.subspan(offset, Blake2s256::kDigestNumBytes));
  }

  size_t num_tail_bytes = num_bytes % Blake2s256::kDigestNumBytes;
  if (num_tail_bytes <= num_spare_bytes_) {
    std::copy(
        spare_bytes_.begin(), spare_bytes_.begin() + num_tail_bytes,
        random_bytes_out.begin() + num_full_blocks * Blake2s256::kDigestNumBytes);
    num_spare_bytes_ -= num_tail_bytes;
    std::copy(spare_bytes_.begin() + num_tail_bytes, spare_bytes_.end(), spare_bytes_.begin());
  } else {
    GetMoreRandomBytesUsingHashWithCounter(
        counter_++,
        random_bytes_out.subspan(num_full_blocks * Blake2s256::kDigestNumBytes, num_tail_bytes));
  }
}

void HashChain::UpdateHashChain(gsl::span<const std::byte> raw_bytes) {
  std::vector<std::byte> mixed_bytes(raw_bytes.size() + Blake2s256::kDigestNumBytes);
  std::copy(hash_.GetDigest().begin(), hash_.GetDigest().end(), mixed_bytes.begin());
  std::copy(raw_bytes.begin(), raw_bytes.end(), mixed_bytes.begin() + Blake2s256::kDigestNumBytes);
  hash_ = Blake2s256::HashBytesWithLength(mixed_bytes);
  num_spare_bytes_ = 0;
  counter_ = 0;
}

void HashChain::GetMoreRandomBytesUsingHashWithCounter(
    const uint64_t counter, gsl::span<std::byte> random_bytes_out) {
  size_t num_bytes = random_bytes_out.size();
  ASSERT_RELEASE(
      num_bytes <= Blake2s256::kDigestNumBytes, "Asked to get more bytes than one digest size.");
  auto prandom_bytes = HashWithCounter(hash_, counter).GetDigest();
  std::copy(prandom_bytes.begin(), prandom_bytes.begin() + num_bytes, random_bytes_out.begin());
  ASSERT_RELEASE(
      num_spare_bytes_ < Blake2s256::kDigestNumBytes + num_bytes,
      "Not enough room in spare bytes buffer. Have " + std::to_string(num_spare_bytes_) +
          " bytes and want to add " + std::to_string(Blake2s256::kDigestNumBytes - num_bytes) +
          " bytes.");
  std::copy(
      prandom_bytes.begin() + num_bytes, prandom_bytes.end(),
      spare_bytes_.begin() + num_spare_bytes_);
  num_spare_bytes_ += (Blake2s256::kDigestNumBytes - num_bytes);
}

Blake2s256 HashChain::HashWithCounter(const Blake2s256& hash, uint64_t counter) {
  std::array<std::byte, 2 * Blake2s256::kDigestNumBytes> data{};
  std::array<std::byte, sizeof(uint64_t)> bytes{};
  Serialize(counter, bytes);

  ASSERT_RELEASE(
      sizeof(uint64_t) <= Blake2s256::kDigestNumBytes,
      "Digest size must be larger than sizeof(uint64_t).");

  std::copy(hash.GetDigest().begin(), hash.GetDigest().end(), data.begin());
  // Copy the counter's serialized 64 bits into the MSB end of the buffer.
  std::copy(bytes.begin(), bytes.end(), data.end() - sizeof(uint64_t));
  return Blake2s256::HashBytesWithLength(data);
}

}  // namespace starkware
