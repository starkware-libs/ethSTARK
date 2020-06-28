/*
  This file implements a prover that proves the following claim:
  "I know a sequence of inputs {w_i} such that H(...H(H(w_0, w_1), w_2) ..., w_n) = p",
  where H is the Rescue hash function, {w_i} are 4-tuples of field elements each and p is a public
  output of the hash (which also consists of 4 field elements).
*/

#include <sys/resource.h>

#include "gflags/gflags.h"

#include "starkware/main/prover_main_helper.h"
#include "starkware/statement/rescue/rescue_statement.h"
#include "starkware/utils/flag_validators.h"
#include "starkware/utils/json.h"
#include "starkware/utils/profiling.h"

DEFINE_string(
    out_file, "",
    "Path to the output file, into which the context and output of the prover will be written.");
DEFINE_validator(out_file, &starkware::ValidateOutputFile);

DEFINE_string(
    prover_config_file, "",
    "Path to the JSON file containing parameters controlling the prover's optimization.");
DEFINE_validator(prover_config_file, &starkware::ValidateInputFile);

DEFINE_bool(generate_annotations, false, "Optional. Generate proof annotations.");

DEFINE_bool(fix_public_input, false, "Optional. Re-compute the public input.");

DEFINE_string(parameter_file, "", "Path to the JSON file containing the proof parameters.");
DEFINE_validator(parameter_file, &starkware::ValidateInputFile);

DEFINE_string(public_input_file, "", "Path to the JSON file containing the public input.");
DEFINE_validator(public_input_file, &starkware::ValidateInputFile);

DEFINE_string(private_input_file, "", "Path to the JSON file containing the private input.");
DEFINE_validator(private_input_file, &starkware::ValidateInputFile);

namespace starkware {

namespace {

/*
  Reads the JSON file specified by the --private_input flag.
*/
JsonValue GetPrivateInput() { return JsonValue::FromFile(FLAGS_private_input_file); }

/*
  Reads the JSON file specified by the --public_input flag.
*/
JsonValue GetPublicInput() { return JsonValue::FromFile(FLAGS_public_input_file); }

/*
  Reads the JSON file specified by the --parameter_file flag.
*/
JsonValue GetParametersInput() { return JsonValue::FromFile(FLAGS_parameter_file); }

/*
  Reads the JSON file specified by the --prover_config_file flag.
*/
JsonValue GetStarkProverConfig() { return JsonValue::FromFile(FLAGS_prover_config_file); }

void DisableCoreDump() {
  struct rlimit rlim {};

  rlim.rlim_cur = rlim.rlim_max = 0;
  setrlimit(RLIMIT_CORE, &rlim);
}

}  // namespace

}  // namespace starkware

int main(int argc, char** argv) {
  using namespace starkware;  // NOLINT
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);  // NOLINT

  const JsonValue public_input = GetPublicInput();
  const JsonValue private_input = GetPrivateInput();

  RescueStatement statement(public_input, private_input);
  ProfilingBlock profiling_block("Prover");

  // Disable core dumps to save storage space.
  DisableCoreDump();

  // If the --fix_public_input flag is true, the public input is evaluated from the private input,
  // and replaces the given public input (which may be empty).
  const JsonValue fixed_public_input =
      FLAGS_fix_public_input ? statement.FixPublicInput() : public_input;
  ProverMainHelper(
      &statement, GetParametersInput(), GetStarkProverConfig(), fixed_public_input, FLAGS_out_file,
      FLAGS_generate_annotations);
  return 0;
}
