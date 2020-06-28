#include "starkware/crypt_tools/blake2s_160.h"

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

using blake2s_160::details::InitDigestFromSpan;

const std::array<std::byte, 12> kHelloWorld =
    MakeByteArray<'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'>();
// This was obtained using python3's hashlib.blake2s(b'Hello World!',digest_size=20).hexdigest().
const std::array<std::byte, Blake2s160::kDigestNumBytes> kResultFull = MakeByteArray<
    0xe6, 0x07, 0x61, 0x97, 0xda, 0xb4, 0xe5, 0x68, 0xb7, 0x25, 0x42, 0x1a, 0x43, 0x56, 0xe1, 0x91,
    0xf4, 0xac, 0x13, 0xab>();

TEST(Blake2s160, HelloWorldHashFull) {
  Blake2s160 hashed_hello_world = Blake2s160::HashBytesWithLength(kHelloWorld);
  EXPECT_EQ(kResultFull, hashed_hello_world.GetDigest());
}

TEST(Blake2s160, HashTwoHashesWithLength) {
  const std::array<std::byte, Blake2s160::kDigestNumBytes> k_result3 = MakeByteArray<
      0x04, 0x2a, 0x4d, 0x55, 0x5a, 0x0a, 0xca, 0x24, 0xaa, 0x20, 0x48, 0x11, 0x60, 0x9c, 0x58,
      0x4e, 0xa9, 0xb2, 0xb4, 0x27>();
  Blake2s160 h1 = Blake2s160::HashBytesWithLength(kHelloWorld);
  Blake2s160 h2 = Blake2s160::HashBytesWithLength(h1.GetDigest());
  Blake2s160 h3 = Blake2s160::Hash(h1, h2);
  EXPECT_EQ(k_result3, h3.GetDigest());
}

TEST(Blake2s160, OutStreamOperator) {
  gsl::span<const std::byte> hello_world_span(kHelloWorld);
  Blake2s160 hashed_hello_world = Blake2s160::HashBytesWithLength(hello_world_span);
  std::stringstream ss;
  ss << hashed_hello_world;
  EXPECT_EQ(hashed_hello_world.ToString(), ss.str());
  EXPECT_EQ("0xe6076197dab4e568b725421a4356e191f4ac13ab", ss.str());
}

TEST(HashUtilsTest, HashBytesWithLength) {
  Prng prng;
  std::array<std::byte, 320> bytes{};
  prng.GetRandomBytes(bytes);

  const Blake2s160 hash1 = Blake2s160::HashBytesWithLength(bytes);
  const Blake2s160 hash2 = Blake2s160::HashBytesWithLength(bytes);

  EXPECT_EQ(hash1, hash2);
}

TEST(HashUtilsTest, InitDigestFromSpan) {
  Prng prng;
  Blake2s160 hash = prng.RandomHash();
  std::array<std::byte, Blake2s160::kDigestNumBytes> bytes{InitDigestFromSpan(hash.GetDigest())};
  EXPECT_EQ(bytes, hash.GetDigest());
}

TEST(HashUtilsTest, BadInput) {
  const auto data_long = std::vector<std::byte>(Blake2s160::kDigestNumBytes + 1);
  const std::string err_str = "Invalid digest initialization length.";

  EXPECT_ASSERT(InitDigestFromSpan(data_long), testing::HasSubstr(err_str));
  EXPECT_ASSERT(Blake2s160::InitDigestTo(data_long), testing::HasSubstr(err_str));

  const auto data_short = std::vector<std::byte>(Blake2s160::kDigestNumBytes - 1);
  EXPECT_ASSERT(InitDigestFromSpan(data_short), testing::HasSubstr(err_str));
  EXPECT_ASSERT(Blake2s160::InitDigestTo(data_short), testing::HasSubstr(err_str));
}

}  // namespace
}  // namespace starkware
