#ifndef STARKWARE_CHANNEL_CHANNEL_STATISTICS_H_
#define STARKWARE_CHANNEL_CHANNEL_STATISTICS_H_

#include <string>

namespace starkware {

class ChannelStatistics {
 public:
  std::string ToString() const {
    std::string stats = "Byte count: " + std::to_string(byte_count) + "\n" +
                        "Hash count: " + std::to_string(hash_count) + "\n" +
                        "Commitment count: " + std::to_string(commitment_count) + "\n" +
                        "Field element count: " + std::to_string(field_element_count) + "\n" +
                        "Data count: " + std::to_string(data_count) + "\n";
    return stats;
  }

  size_t byte_count = 0;
  size_t hash_count = 0;
  size_t commitment_count = 0;
  size_t field_element_count = 0;
  size_t data_count = 0;
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_CHANNEL_STATISTICS_H_
