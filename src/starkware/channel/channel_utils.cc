#include "starkware/channel/channel_utils.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include "glog/logging.h"

#include "starkware/math/math.h"
#include "starkware/utils/serialization.h"

namespace starkware {

uint64_t ChannelUtils::GetRandomNumber(uint64_t upper_bound, Prng* prng) {
  std::array<std::byte, sizeof(uint64_t)> raw_bytes{};
  prng->GetRandomBytes(raw_bytes);
  uint64_t number = Deserialize(raw_bytes);
  // The modulo operation will be random enough if the upper bound is small compared to max uint64.
  // Ensure less than 1/2^16 non-uniformity in the modulo operation.
  ASSERT_RELEASE(upper_bound < Pow2(48), "upper_bound must be smaller than 2^48.");
  return number % upper_bound;
}

}  // namespace starkware
