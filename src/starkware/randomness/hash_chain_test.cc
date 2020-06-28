#include <map>
#include <utility>

#include "gtest/gtest.h"

#include "starkware/crypt_tools/blake2s_160.h"
#include "starkware/randomness/hash_chain.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {
namespace {

/*
  Test constants.
*/
std::vector<std::byte> random_bytes_1st_blake2s160 = {
    std::byte{0x30}, std::byte{0xEF}, std::byte{0xDF}, std::byte{0x05},
    std::byte{0x17}, std::byte{0x99}, std::byte{0x13}, std::byte{0x1D}};

std::vector<std::byte> random_bytes_1000th_blake2s160 = {
    std::byte{0x61}, std::byte{0x7B}, std::byte{0xAB}, std::byte{0x09},
    std::byte{0x52}, std::byte{0xF2}, std::byte{0xCC}, std::byte{0x41}};

std::vector<std::byte> random_bytes_1001st_blake2s160 = {
    std::byte{0x26}, std::byte{0x7A}, std::byte{0x33}, std::byte{0x9F},
    std::byte{0xE7}, std::byte{0x3A}, std::byte{0xEB}, std::byte{0x3C}};

std::map<size_t, std::vector<std::byte>> random_bytes_blake2s160 = {
    std::make_pair(1, random_bytes_1st_blake2s160),
    std::make_pair(1000, random_bytes_1000th_blake2s160),
    std::make_pair(1001, random_bytes_1001st_blake2s160)};

const std::map<size_t, std::vector<std::byte>> kExpectedRandomByteVector = random_bytes_blake2s160;

TEST(HashChainTest, HashChGetRandoms) {
  std::array<std::byte, sizeof(uint64_t)> bytes_1{};
  std::array<std::byte, sizeof(uint64_t)> bytes_2{};

  HashChain hash_ch_1 = HashChain(bytes_1);
  HashChain hash_ch_2 = HashChain(bytes_2);
  Blake2s160 stat1 = hash_ch_1.GetHashChainState();
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
TEST(HashChain, Blake2s160HashChInitUpdate) {
  const std::array<std::byte, 12> k_hello_world =
      MakeByteArray<'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'>();

  HashChain hash_ch_1(k_hello_world);
  HashChain hash_ch_2 = HashChain();
  ASSERT_NE(hash_ch_2.GetHashChainState(), hash_ch_1.GetHashChainState());
  const std::array<std::byte, Blake2s160::kDigestNumBytes> exp_hw_hash = MakeByteArray<
      0xE6, 0x07, 0x61, 0x97, 0xDA, 0xB4, 0xE5, 0x68, 0xB7, 0x25, 0x42, 0x1A, 0x43, 0x56, 0xE1,
      0x91, 0xF4, 0xAC, 0x13, 0xAB>();
  ASSERT_EQ(exp_hw_hash, hash_ch_1.GetHashChainState().GetDigest());
}

}  // namespace
}  // namespace starkware
