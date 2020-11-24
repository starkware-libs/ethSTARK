#include "starkware/statement/ziggy/ziggy_statement.h"

#include <algorithm>
#include <utility>

#include "starkware/algebra/field_operations.h"
#include "starkware/utils/json_builder.h"

namespace starkware {

ZiggyStatement::ZiggyStatement(
    const JsonValue& public_input, std::optional<JsonValue> private_input)
    : Statement(std::move(private_input)),
      public_key_({
          public_input["public_key"][0].AsFieldElement<BaseFieldElement>(),
          public_input["public_key"][1].AsFieldElement<BaseFieldElement>(),
          public_input["public_key"][2].AsFieldElement<BaseFieldElement>(),
          public_input["public_key"][3].AsFieldElement<BaseFieldElement>(),
      }),
      message_(public_input["message"].AsString()) {
  if (private_input_.has_value()) {
    PrivateKeyT private_key;
    (*private_input_)["private_key"].AsBytesFromHexString(private_key);
    private_key_.emplace(private_key);
    Prng secret_preimage_prng(GetSecretPreimageSeed());
    const SecretPreimageT secret_preimage = {
        BaseFieldElement::RandomElement(&secret_preimage_prng),
        BaseFieldElement::RandomElement(&secret_preimage_prng),
        BaseFieldElement::RandomElement(&secret_preimage_prng),
        BaseFieldElement::RandomElement(&secret_preimage_prng),
        BaseFieldElement::RandomElement(&secret_preimage_prng),
        BaseFieldElement::RandomElement(&secret_preimage_prng),
        BaseFieldElement::RandomElement(&secret_preimage_prng),
        BaseFieldElement::RandomElement(&secret_preimage_prng),
    };
    secret_preimage_.emplace(secret_preimage);
  }
}

const Air& ZiggyStatement::GetAir(bool is_zero_knowledge, size_t n_queries) {
  ASSERT_RELEASE(is_zero_knowledge, "Ziggy proof must be zero knowledge.");
  air_ = std::make_unique<ZiggyAir>(public_key_, is_zero_knowledge, n_queries);
  return *air_;
}

const std::vector<std::byte> ZiggyStatement::GetInitialHashChainSeed() const {
  using std::literals::string_literals::operator""s;
  const std::string ziggy_string = "Ziggy\x00"s;
  const size_t public_key_element_bytes = BaseFieldElement::SizeInBytes();
  std::vector<std::byte> randomness_seed(
      ziggy_string.size() + public_key_element_bytes * ZiggyAir::kWordSize +
      message_.size());
  std::transform(
      ziggy_string.begin(), ziggy_string.end(), randomness_seed.begin(),
      [](char c) { return std::byte(c); });
  for (size_t i = 0; i < public_key_.size(); i++) {
    const auto& val = public_key_[i];
    val.ToBytes(
        gsl::make_span(randomness_seed)
            .subspan(
                ziggy_string.size() + i * public_key_element_bytes, public_key_element_bytes));
  }
  std::transform(
      message_.begin(), message_.end(),
      randomness_seed.begin() + ziggy_string.size() +
          public_key_element_bytes * ZiggyAir::kWordSize,
      [](char c) { return std::byte(c); });
  return randomness_seed;
}

const std::vector<std::byte> ZiggyStatement::GetZeroKnowledgeHashChainSeed() const {
  using std::literals::string_literals::operator""s;
  const std::string& private_string = "Ziggy private seed\x00"s;
  std::vector<std::byte> randomness_seed(
      private_string.size() + private_key_->size() + message_.size());
  std::transform(private_string.begin(), private_string.end(), randomness_seed.begin(), [](char c) {
    return std::byte(c);
  });
  std::copy(
      private_key_->begin(), private_key_->end(), randomness_seed.begin() + private_string.size());
  std::transform(
      message_.begin(), message_.end(),
      randomness_seed.begin() + private_string.size() + private_key_->size(),
      [](char c) { return std::byte(c); });
  return randomness_seed;
}

const std::vector<std::byte> ZiggyStatement::GetSecretPreimageSeed() const {
  using std::literals::string_literals::operator""s;
  const std::string& secret_preimage_string = "Ziggy secret preimage seed\x00"s;
  std::vector<std::byte> randomness_seed(secret_preimage_string.size() + private_key_->size());
  std::transform(
      secret_preimage_string.begin(), secret_preimage_string.end(), randomness_seed.begin(),
      [](char c) { return std::byte(c); });
  std::copy(
      private_key_->begin(), private_key_->end(),
      randomness_seed.begin() + secret_preimage_string.size());
  return randomness_seed;
}

Trace ZiggyStatement::GetTrace(Prng* prng) const {
  ASSERT_RELEASE(
      air_ != nullptr,
      "Cannot construct trace without a fully initialized AIR instance. Please call GetAir() prior "
      "to GetTrace().");
  ASSERT_RELEASE(secret_preimage_.has_value(), "secret_preimage_ must have a value.");
  ASSERT_RELEASE(prng != nullptr, "prng should not be null when using zero knowledge.");
  return air_->GetTrace(*secret_preimage_, prng);
}

JsonValue ZiggyStatement::FixPublicInput() {
  JsonBuilder root;
  public_key_ = ZiggyAir::PublicInputFromPrivateInput(*secret_preimage_);
  for (auto element : public_key_) {
    root["public_key"].Append(element.ToString());
  }
  root["message"] = message_;
  return root.Build();
}

}  // namespace starkware
