#ifndef STARKWARE_MAIN_PROVER_MAIN_HELPER_H_
#define STARKWARE_MAIN_PROVER_MAIN_HELPER_H_

#include <string>
#include <vector>

#include "starkware/statement/statement.h"
#include "starkware/utils/json.h"

namespace starkware {

/*
  A helper function for writing the main() function of STARK provers.
  Creates a new prover channel and a new StarkProver that writes to this channel.
  Returns a byte vector containing the proof from the channel. If an out_file_name value is
  provided, writes the entire prover's context to that file.
*/
std::vector<std::byte> ProverMainHelper(
    Statement* statement, const JsonValue& parameters, const JsonValue& stark_config_json,
    const JsonValue& public_input, const std::string& out_file_name = "",
    bool generate_annotations = false);

}  // namespace starkware

#endif  // STARKWARE_MAIN_PROVER_MAIN_HELPER_H_
