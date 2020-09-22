#include <map>
#include <utility>

#include "gtest/gtest.h"

#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/randomness/hash_chain.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {
namespace {

/*
  Test constants.
*/
std::vector<std::byte> random_bytes_1st_blake2s256 = {
    std::byte{0xF4}, std::byte{0x07}, std::byte{0x5C}, std::byte{0x07},
    std::byte{0x91}, std::byte{0xC2}, std::byte{0x11}, std::byte{0x01}};

std::vector<std::byte> random_bytes_1000th_blake2s256 = {
    std::byte{0x89}, std::byte{0xE1}, std::byte{0x86}, std::byte{0xD5},
    std::byte{0x47}, std::byte{0x15}, std::byte{0x81}, std::byte{0x86}};

std::vector<std::byte> random_bytes_1001st_blake2s256 = {
    std::byte{0xD4}, std::byte{0x94}, std::byte{0x5A}, std::byte{0x65},
    std::byte{0x25}, std::byte{0x0A}, std::byte{0x61}, std::byte{0xB8}};

std::map<size_t, std::vector<std::byte>> random_bytes_blake2s256 = {
    std::make_pair(1, random_bytes_1st_blake2s256),
    std::make_pair(1000, random_bytes_1000th_blake2s256),
    std::make_pair(1001, random_bytes_1001st_blake2s256)};

const std::map<size_t, std::vector<std::byte>> kExpectedRandomByteVector = random_bytes_blake2s256;

TEST(HashChainTest, HashChGetRandoms) {
  std::array<std::byte, sizeof(uint64_t)> bytes_1{};
  std::array<std::byte, sizeof(uint64_t)> bytes_2{};

  HashChain hash_ch_1 = HashChain(bytes_1);
  HashChain hash_ch_2 = HashChain(bytes_2);
  Blake2s256 stat1 = hash_ch_1.GetHashChainState();
  hash_ch_1.GetRandomBytes(bytes_1);
  hash_ch_2.GetRandomBytes(bytes_2);

  for (int i = 0; i < 1000; ++i) {
    hash_ch_1.GetRandomBytes(bytes_1);
    hash_ch_2.GetRandomBytes(bytes_2);
  }

  EXPECT_EQ(stat1, hash_ch_1.GetHashChainState());
  EXPECT_EQ(stat1, hash_ch_2.GetHashChainState());
  EXPECT_EQ(bytes_1, bytes_2);
}

TEST(HashChainTest, PyHashChainUpdateParity) {
  const std::array<std::byte, 8> dead_beef_bytes =
      MakeByteArray<0x00, 0x00, 0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF>();
  const std::array<std::byte, 8> daba_daba_da_bytes =
      MakeByteArray<0x00, 0x00, 0x00, 0xDA, 0xBA, 0xDA, 0xBA, 0xDA>();

  std::array<std::byte, sizeof(uint64_t)> bytes_1{};
  auto hash_ch = HashChain(dead_beef_bytes);

  hash_ch.GetRandomBytes(bytes_1);
  std::vector<std::byte> bytes_1v(bytes_1.begin(), bytes_1.end());
  EXPECT_EQ(kExpectedRandomByteVector.at(1), bytes_1v);

  for (int i = 1; i < 1000; ++i) {
    hash_ch.GetRandomBytes(bytes_1);
  }
  bytes_1v = std::vector<std::byte>(bytes_1.begin(), bytes_1.end());
  EXPECT_EQ(kExpectedRandomByteVector.at(1000), bytes_1v);

  hash_ch.UpdateHashChain(daba_daba_da_bytes);
  hash_ch.GetRandomBytes(bytes_1);

  bytes_1v = std::vector<std::byte>(bytes_1.begin(), bytes_1.end());
  EXPECT_EQ(kExpectedRandomByteVector.at(1001), bytes_1v);
}

// Ensure HashChain is initialized identically to the python counterpart.
TEST(HashChain, Blake2s256HashChInitUpdate) {
  const std::array<std::byte, 12> k_hello_world =
      MakeByteArray<'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'>();

  HashChain hash_ch_1(k_hello_world);
  HashChain hash_ch_2 = HashChain();
  ASSERT_NE(hash_ch_2.GetHashChainState(), hash_ch_1.GetHashChainState());
  const std::array<std::byte, Blake2s256::kDigestNumBytes> exp_hw_hash = MakeByteArray<
      0xBE, 0x8C, 0x67, 0x77, 0xE8, 0x8D, 0x28, 0x7D, 0xD9, 0x27, 0x97, 0x53, 0x27, 0xDD, 0x42,
      0x14, 0xD1, 0x99, 0xA1, 0xA1, 0xB6, 0x7F, 0xE2, 0xE2, 0x66, 0x66, 0xCC, 0x33, 0x65, 0x33,
      0x66, 0x6A>();
  ASSERT_EQ(exp_hw_hash, hash_ch_1.GetHashChainState().GetDigest());
}

}  // namespace
}  // namespace starkware
