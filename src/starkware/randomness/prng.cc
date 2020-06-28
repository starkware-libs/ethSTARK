#include "starkware/randomness/prng.h"

#include <chrono>

#include "gflags/gflags.h"
#include "glog/logging.h"

#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/serialization.h"
#include "starkware/utils/to_from_string.h"

DEFINE_string(override_random_seed, "", "override seed for prng");

namespace starkware {

namespace {

std::array<std::byte, sizeof(uint64_t)> SeedFromSystemTime() {
  uint64_t seed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();

  std::array<std::byte, sizeof(uint64_t)> seed_bytes{};
  Serialize(seed, seed_bytes);

  return seed_bytes;
}

std::array<std::byte, sizeof(uint64_t)> SeedPrintoutToBytes(const std::string& printout) {
  std::array<std::byte, sizeof(uint64_t)> seed_bytes{};
  HexStringToBytes(printout, seed_bytes);
  return seed_bytes;
}

}  // namespace

Prng::Prng() {
  const std::array<std::byte, sizeof(uint64_t)> seed_bytes =
      FLAGS_override_random_seed.empty() ? SeedFromSystemTime()
                                         : SeedPrintoutToBytes(FLAGS_override_random_seed);
  const std::string seed_string = BytesToHexString(seed_bytes);
  LOG(INFO) << "Seeding PRNG with " << seed_string << ".";
  ASSERT_RELEASE(
      SeedPrintoutToBytes(seed_string) == seed_bytes, "Randomness not reproducible from printout.");
  hash_chain_.InitHashChain(seed_bytes);
}

}  // namespace starkware
