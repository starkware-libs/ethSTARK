#include "starkware/algebra/fields/base_field_element.h"

#include <cstddef>
#include <vector>

#include "starkware/utils/serialization.h"
#include "starkware/utils/to_from_string.h"

namespace starkware {

void BaseFieldElement::ToBytes(gsl::span<std::byte> span_out) const {
  ASSERT_RELEASE(
      span_out.size() == SizeInBytes(), "Destination span size mismatches field element size.");
  Serialize(value_, span_out);
}

BaseFieldElement BaseFieldElement::FromBytes(gsl::span<const std::byte> bytes) {
  ASSERT_RELEASE(
      bytes.size() == SizeInBytes(), "Source span size mismatches field element size, expected " +
                                         std::to_string(SizeInBytes()) + ", got " +
                                         std::to_string(bytes.size()));
  return BaseFieldElement(Deserialize(bytes));
}

BaseFieldElement BaseFieldElement::FromString(const std::string& s) {
  std::array<std::byte, SizeInBytes()> as_bytes{};
  HexStringToBytes(s, as_bytes);
  uint64_t res =
      BaseFieldElement::MontgomeryMul(Deserialize(as_bytes), BaseFieldElement::kMontgomeryRSquared);
  return BaseFieldElement(res);
}

std::string BaseFieldElement::ToString() const {
  std::array<std::byte, SizeInBytes()> as_bytes{};
  Serialize(ToStandardForm(), as_bytes);
  return BytesToHexString(as_bytes);
}

uint64_t BaseFieldElement::ToStandardForm() const { return MontgomeryMul(value_, 1); }

BaseFieldElement BaseFieldElement::RandomElement(Prng* prng) {
  // Note that there is no need to call FromUint here because uniform distribution
  // is preserved under the Montgomery transformation (multiplication by kMontgomeryR^-1).
  constexpr uint64_t kReleventBits = (Pow2(kModulusBits + 1)) - 1;

  std::array<std::byte, SizeInBytes()> bytes{};
  uint64_t deserialization;

  do {
    prng->GetRandomBytes(bytes);
    deserialization = Deserialize(bytes) & kReleventBits;
  } while (deserialization >= kModulus);

  return BaseFieldElement(deserialization);
}

}  // namespace starkware
