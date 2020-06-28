#ifndef STARKWARE_MAIN_VERIFIER_MAIN_HELPER_H_
#define STARKWARE_MAIN_VERIFIER_MAIN_HELPER_H_

#include <string>
#include <vector>

#include "starkware/statement/statement.h"
#include "starkware/utils/json.h"

namespace starkware {

/*
  A helper function for writing the main() function for STARK verifiers.
  Creates a new StarkVerifier according to the provided statement, proof and parameters.
  Returns true if the proof is successfully verified by the verifier, false otherwise. If an
  annotation_file_name value is provided, writes the verifier's channel output to that file.
*/
bool VerifierMainHelper(
    Statement* statement, const std::vector<std::byte>& proof, const JsonValue& parameters,
    const std::string& annotation_file_name);

}  // namespace starkware

#endif  // STARKWARE_MAIN_VERIFIER_MAIN_HELPER_H_
