#ifndef STARKWARE_CHANNEL_PROVER_CHANNEL_MOCK_H_
#define STARKWARE_CHANNEL_PROVER_CHANNEL_MOCK_H_

#include <string>
#include <vector>

#include "gmock/gmock.h"

#include "starkware/channel/prover_channel.h"

namespace starkware {

class ProverChannelMock : public ProverChannel {
 public:
  MOCK_METHOD1(SendFieldElementImpl, void(const BaseFieldElement& value));
  MOCK_METHOD1(SendFieldElementImpl, void(const ExtensionFieldElement& value));
  MOCK_METHOD1(SendFieldElementSpanImpl, void(gsl::span<const ExtensionFieldElement> values));

  MOCK_METHOD0(ReceiveFieldElementImpl, ExtensionFieldElement());
  MOCK_METHOD1(ReceiveNumberImpl, uint64_t(uint64_t upper_bound));

  MOCK_METHOD1(SendBytes, void(gsl::span<const std::byte> raw_bytes));
  MOCK_METHOD1(ReceiveBytes, std::vector<std::byte>(const size_t num_bytes));
  MOCK_METHOD1(ApplyProofOfWork, void(size_t));
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_PROVER_CHANNEL_MOCK_H_
