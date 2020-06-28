#include "starkware/utils/serialization.h"

#include <cstddef>
#include <limits>

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::ElementsAreArray;

TEST(SerializationTest, SerializeDeserialize) {
  Prng prng;
  const auto a = prng.UniformInt<uint64_t>(
      std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max());
  std::array<std::byte, sizeof(uint64_t)> buf{};
  Serialize(a, buf);
  const auto b = Deserialize(buf);
  EXPECT_EQ(a, b);
}

TEST(SerializationTest, BadInput) {
  std::array<std::byte, sizeof(uint32_t)> bad_arr_size{};
  EXPECT_ASSERT(
      Serialize(0x37ffd4ab5e008810, bad_arr_size),
      testing::HasSubstr("Destination span size mismatches"));

  EXPECT_ASSERT(Deserialize(bad_arr_size), testing::HasSubstr("Source span size mismatches"));
}

TEST(SerializationTest, BigEndianness) {
  std::array<std::byte, sizeof(uint64_t)> arr{};
  Serialize(0x37ffd4ab5e008810, arr);
  std::array<uint8_t, sizeof(uint64_t)> expected{0x37, 0xff, 0xd4, 0xab, 0x5e, 0x00, 0x88, 0x10};
  EXPECT_THAT(gsl::make_span(expected).as_span<std::byte>(), ElementsAreArray(arr));
}

}  // namespace
}  // namespace starkware
