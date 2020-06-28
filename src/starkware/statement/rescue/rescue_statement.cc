#include "starkware/statement/rescue/rescue_statement.h"

#include <utility>

#include "starkware/utils/json_builder.h"

#include "starkware/algebra/field_operations.h"

namespace starkware {

RescueStatement::RescueStatement(
    const JsonValue& public_input, std::optional<JsonValue> private_input)
    : Statement(std::move(private_input)),
      output_({public_input["output"][0].AsFieldElement<BaseFieldElement>(),
               public_input["output"][1].AsFieldElement<BaseFieldElement>(),
               public_input["output"][2].AsFieldElement<BaseFieldElement>(),
               public_input["output"][3].AsFieldElement<BaseFieldElement>()}),
      chain_length_(public_input["chain_length"].AsSizeT()) {
  ASSERT_RELEASE(chain_length_ > 0, "Chain length must be positive.");
  ASSERT_RELEASE(
      chain_length_ % RescueAir::kHashesPerBatch == 0,
      "Chain length must be divisible by " + std::to_string(RescueAir::kHashesPerBatch) + ".");
  if (private_input_.has_value()) {
    witness_ = GetWitness();
  }
}

const Air& RescueStatement::GetAir() {
  air_ = std::make_unique<RescueAir>(output_, chain_length_);
  return *air_;
}

const std::vector<std::byte> RescueStatement::GetInitialHashChainSeed() const {
  const size_t output_element_bytes = BaseFieldElement::SizeInBytes();
  const size_t chain_length_bytes = sizeof(uint64_t);
  std::vector<std::byte> randomness_seed(
      output_element_bytes * RescueAir::kWordSize + chain_length_bytes);
  for (size_t i = 0; i < output_.size(); i++) {
    const auto& val = output_[i];
    val.ToBytes(
        gsl::make_span(randomness_seed).subspan(i * output_element_bytes, output_element_bytes));
  }
  Serialize(
      uint64_t(chain_length_),
      gsl::make_span(randomness_seed)
          .subspan(RescueAir::kWordSize * output_element_bytes, chain_length_bytes));
  return randomness_seed;
}

Trace RescueStatement::GetTrace() const {
  ASSERT_RELEASE(
      air_ != nullptr,
      "Cannot construct trace without a fully initialized AIR instance. Please call GetAir() prior "
      "to GetTrace().");
  ASSERT_RELEASE(witness_.has_value(), "witness_ must have a value.");
  return air_->GetTrace(*witness_);
}

typename RescueStatement::WitnessT RescueStatement::ParseWitness(const JsonValue& witness) {
  typename RescueStatement::WitnessT res;
  res.reserve(witness.ArrayLength());
  for (size_t i = 0; i < witness.ArrayLength(); i++) {
    res.push_back({witness[i][0].AsFieldElement<BaseFieldElement>(),
                   witness[i][1].AsFieldElement<BaseFieldElement>(),
                   witness[i][2].AsFieldElement<BaseFieldElement>(),
                   witness[i][3].AsFieldElement<BaseFieldElement>()});
  }
  return res;
}

typename RescueStatement::WitnessT RescueStatement::GetWitness() const {
  ASSERT_RELEASE(private_input_.has_value(), "Missing private input.");
  const JsonValue witness = (*private_input_)["witness"];
  ASSERT_RELEASE(
      witness.ArrayLength() == chain_length_ + 1,
      "Witness length (" + std::to_string(witness.ArrayLength()) +
          ") must be equal to chain_length + 1 (" + std::to_string(chain_length_ + 1) + ").");
  return ParseWitness(witness);
}

JsonValue RescueStatement::GetPublicInputJsonValueFromPrivateInput(const JsonValue& private_input) {
  JsonBuilder root;
  const JsonValue private_witness = private_input["witness"];
  uint64_t chain_length = private_witness.ArrayLength() - 1;
  typename RescueStatement::WitnessT witness = RescueStatement::ParseWitness(private_witness);
  typename RescueAir::WordT output = RescueAir::PublicInputFromPrivateInput(witness);
  root["chain_length"] = chain_length;
  for (auto element : output) {
    root["output"].Append(element.ToString());
  }
  return root.Build();
}

JsonValue RescueStatement::FixPublicInput() {
  JsonBuilder root;
  output_ = RescueAir::PublicInputFromPrivateInput(*witness_);
  root["chain_length"] = chain_length_;
  for (auto element : output_) {
    root["output"].Append(element.ToString());
  }
  return root.Build();
}

}  // namespace starkware
