#include "starkware/channel/prover_channel.h"

#include <algorithm>
#include <cstddef>

#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {
namespace {

class ProverChannelTest : public ::testing::Test {
 public:
  Prng prng;
  const Prng channel_prng{};
  std::vector<std::byte> RandomByteVector(size_t length) { return prng.RandomByteVector(length); }
};

TEST_F(ProverChannelTest, ReceivingBytes) {
  ProverChannel channel(this->channel_prng.Clone());

  for (size_t size : {4, 8, 16, 18, 32, 33, 63, 64, 65}) {
    std::vector<std::byte> some_bytes = channel.ReceiveBytes(size);
    EXPECT_EQ(some_bytes.size(), size);
  }
}

TEST_F(ProverChannelTest, RecurringCallsYieldUniformDistribution_Statistical) {
  // In this test we don't use a time based seed to avoid inaccurate EXPECT_NEAR and EXPECT_GT.
  Prng channel_prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  ProverChannel channel(channel_prng.Clone());

  std::array<uint64_t, 10> histogram = {};
  for (size_t i = 0; i < 10000; ++i) {
    histogram.at(channel.ReceiveNumber(10))++;
  }

  for (size_t i = 0; i < 10; ++i) {
    EXPECT_NEAR(1000, histogram.at(i), 98) << "i=" << i;
  }

  uint64_t max_random_number = 0;
  for (size_t i = 0; i < 1000; ++i) {
    uint64_t random_number = channel.ReceiveNumber(10000000);
    max_random_number = std::max(max_random_number, random_number);
  }
  EXPECT_EQ(9999940, max_random_number);  // Empirical result. Should be roughly 9999000.
}

TEST_F(ProverChannelTest, SendingMessageAffectsRandomness) {
  ProverChannel channel1(this->channel_prng.Clone());
  ProverChannel channel2(this->channel_prng.Clone());

  EXPECT_EQ(channel1.ReceiveNumber(10000000), channel2.ReceiveNumber(10000000));
  channel1.SendFieldElement(BaseFieldElement::FromUint(1));
  EXPECT_NE(channel1.ReceiveNumber(10000000), channel2.ReceiveNumber(10000000));
}

TEST_F(ProverChannelTest, SendingMessageAffectsRandomness2) {
  ProverChannel channel1(this->channel_prng.Clone());
  ProverChannel channel2(this->channel_prng.Clone());

  EXPECT_EQ(channel1.ReceiveNumber(10000000), channel2.ReceiveNumber(10000000));
  channel1.SendFieldElement(BaseFieldElement::FromUint(1));
  channel2.SendFieldElement(BaseFieldElement::FromUint(2));
  EXPECT_NE(channel1.ReceiveNumber(10000000), channel2.ReceiveNumber(10000000));
}

}  // namespace
}  // namespace starkware
