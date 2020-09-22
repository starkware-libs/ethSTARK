#include "starkware/crypt_tools/blake2s_256.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {
namespace {

using blake2s_256::details::InitDigestFromSpan;

const std::array<std::byte, 12> kHelloWorld =
    MakeByteArray<'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'>();
// This was obtained using python3's hashlib.blake2s(b'Hello World!',digest_size=32).hexdigest().
const std::array<std::byte, Blake2s256::kDigestNumBytes> kResultFull = MakeByteArray<
    0xbe, 0x8c, 0x67, 0x77, 0xe8, 0x8d, 0x28, 0x7d, 0xd9, 0x27, 0x97, 0x53, 0x27, 0xdd, 0x42, 0x14,
    0xd1, 0x99, 0xa1, 0xa1, 0xb6, 0x7f, 0xe2, 0xe2, 0x66, 0x66, 0xcc, 0x33, 0x65, 0x33, 0x66,
    0x6a>();

TEST(Blake2s256, HelloWorldHashFull) {
  Blake2s256 hashed_hello_world = Blake2s256::HashBytesWithLength(kHelloWorld);
  EXPECT_EQ(kResultFull, hashed_hello_world.GetDigest());
}

TEST(Blake2s256, HashTwoHashesWithLength) {
  const std::array<std::byte, Blake2s256::kDigestNumBytes> k_result3 = MakeByteArray<
      0x2E, 0x51, 0xDD, 0x07, 0x53, 0xF7, 0x55, 0x2D, 0xD3, 0x0D, 0xC5, 0xA0, 0x49, 0xB9, 0x6F,
      0x24, 0xFE, 0xDE, 0x8F, 0x36, 0x3F, 0x19, 0xA8, 0x73, 0x86, 0x05, 0x6C, 0x40, 0x94, 0x40,
      0x6B, 0x68>();
  Blake2s256 h1 = Blake2s256::HashBytesWithLength(kHelloWorld);
  Blake2s256 h2 = Blake2s256::HashBytesWithLength(h1.GetDigest());
  Blake2s256 h3 = Blake2s256::Hash(h1, h2);
  EXPECT_EQ(k_result3, h3.GetDigest());
}

TEST(Blake2s256, OutStreamOperator) {
  gsl::span<const std::byte> hello_world_span(kHelloWorld);
  Blake2s256 hashed_hello_world = Blake2s256::HashBytesWithLength(hello_world_span);
  std::stringstream ss;
  ss << hashed_hello_world;
  EXPECT_EQ(hashed_hello_world.ToString(), ss.str());
  EXPECT_EQ("0xbe8c6777e88d287dd927975327dd4214d199a1a1b67fe2e26666cc336533666a", ss.str());
}

TEST(HashUtilsTest, HashBytesWithLength) {
  Prng prng;
  std::array<std::byte, 320> bytes{};
  prng.GetRandomBytes(bytes);

  const Blake2s256 hash1 = Blake2s256::HashBytesWithLength(bytes);
  const Blake2s256 hash2 = Blake2s256::HashBytesWithLength(bytes);

  EXPECT_EQ(hash1, hash2);
}

TEST(HashUtilsTest, InitDigestFromSpan) {
  Prng prng;
  Blake2s256 hash = prng.RandomHash();
  std::array<std::byte, Blake2s256::kDigestNumBytes> bytes{InitDigestFromSpan(hash.GetDigest())};
  EXPECT_EQ(bytes, hash.GetDigest());
}

TEST(HashUtilsTest, BadInput) {
  const auto data_long = std::vector<std::byte>(Blake2s256::kDigestNumBytes + 1);
  const std::string err_str = "Invalid digest initialization length.";

  EXPECT_ASSERT(InitDigestFromSpan(data_long), testing::HasSubstr(err_str));
  EXPECT_ASSERT(Blake2s256::InitDigestTo(data_long), testing::HasSubstr(err_str));

  const auto data_short = std::vector<std::byte>(Blake2s256::kDigestNumBytes - 1);
  EXPECT_ASSERT(InitDigestFromSpan(data_short), testing::HasSubstr(err_str));
  EXPECT_ASSERT(Blake2s256::InitDigestTo(data_short), testing::HasSubstr(err_str));
}

}  // namespace
}  // namespace starkware
