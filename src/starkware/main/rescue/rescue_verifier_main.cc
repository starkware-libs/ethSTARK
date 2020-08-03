/*
  Verifies a Rescue hash chain proof. See rescue_prover_main.cc for more details.
*/

#include <memory>

#include "gflags/gflags.h"
#include "glog/logging.h"

#include "starkware/main/verifier_main_helper.h"
#include "starkware/statement/rescue/rescue_statement.h"
#include "starkware/utils/flag_validators.h"
#include "starkware/utils/json.h"

DEFINE_string(in_file, "", "Path to the input file.");
DEFINE_validator(in_file, &starkware::ValidateInputFile);

DEFINE_string(
    annotation_file, "",
    "Optional. Path to the output file that will contain the annotated proof.");
DEFINE_validator(annotation_file, &starkware::ValidateOptionalOutputFile);

namespace starkware {

namespace {

struct VerifierInput {
  JsonValue public_input;
  JsonValue parameters;
  std::vector<std::byte> proof;
};

VerifierInput GetVerifierInput() {
  JsonValue input = JsonValue::FromFile(FLAGS_in_file);
  std::string proof_hex = input["proof_hex"].AsString();
  ASSERT_RELEASE(!proof_hex.empty(), "Proof must not be empty.");
  std::vector<std::byte> proof((proof_hex.size() - 1) / 2);
  starkware::HexStringToBytes(proof_hex, proof);
  return {input["public_input"], input["proof_parameters"], proof};
}

}  // namespace

}  // namespace starkware

int main(int argc, char** argv) {
  using namespace starkware;  // NOLINT
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);  // NOLINT

  const VerifierInput verifier_input = GetVerifierInput();

  RescueStatement statement(verifier_input.public_input, std::nullopt);

  bool result = starkware::VerifierMainHelper(
      &statement, verifier_input.proof, verifier_input.parameters, FLAGS_annotation_file);

  if (result) {
    LOG(INFO) << "Proof verified successfully.";
  } else {
    LOG(ERROR) << "Invalid proof.";
  }

  return result ? 0 : 1;
}
