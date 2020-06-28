#ifndef STARKWARE_CHANNEL_VERIFIER_CHANNEL_MOCK_H_
#define STARKWARE_CHANNEL_VERIFIER_CHANNEL_MOCK_H_

#include <string>
#include <vector>

#include "gmock/gmock.h"

#include "starkware/channel/verifier_channel.h"

namespace starkware {

class VerifierChannelMock : public VerifierChannel {
 public:
  MOCK_METHOD1(SendNumberImpl, void(uint64_t number));
  MOCK_METHOD1(SendFieldElementImpl, void(const ExtensionFieldElement& value));
  MOCK_METHOD1(GetAndSendRandomNumberImpl, uint64_t(uint64_t upper_bound));
  MOCK_METHOD0(GetAndSendRandomFieldElementImpl, ExtensionFieldElement());
  MOCK_METHOD0(ReceiveBaseFieldElementImpl, BaseFieldElement());
  MOCK_METHOD0(ReceiveExtensionFieldElementImpl, ExtensionFieldElement());
  MOCK_METHOD1(ReceiveFieldElementSpanImpl, void(gsl::span<ExtensionFieldElement> span));

  MOCK_METHOD1(SendBytes, void(gsl::span<const std::byte> raw_bytes));
  MOCK_METHOD1(ReceiveBytes, std::vector<std::byte>(const size_t num_bytes));
  MOCK_METHOD1(GetRandomNumber, uint64_t(uint64_t upper_bound));
  MOCK_METHOD0(GetRandomFieldElement, ExtensionFieldElement());
  MOCK_METHOD1(ApplyProofOfWork, void(size_t));
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_VERIFIER_CHANNEL_MOCK_H_
