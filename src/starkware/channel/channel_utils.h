#ifndef STARKWARE_CHANNEL_CHANNEL_UTILS_H_
#define STARKWARE_CHANNEL_CHANNEL_UTILS_H_

#include <vector>

#include "starkware/randomness/prng.h"

namespace starkware {

class ChannelUtils {
 public:
  static uint64_t GetRandomNumber(uint64_t upper_bound, Prng* prng);
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_CHANNEL_UTILS_H_
