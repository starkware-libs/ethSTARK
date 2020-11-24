#ifndef STARKWARE_UTILS_INPUT_UTILS_H_
#define STARKWARE_UTILS_INPUT_UTILS_H_

#include <string>
#include <vector>

#include "starkware/utils/json.h"

namespace starkware {

/*
  Returns a JSON configuration for the prover.
*/
JsonValue GetProverConfigJson(size_t constraint_polynomial_task_size = 256);

/*
  Returns a JSON configuration for the prover and the verifier.
*/
JsonValue GetParametersJson(
    uint64_t trace_length, uint64_t log_n_cosets, uint64_t security_bits,
    uint64_t proof_of_work_bits, const std::vector<size_t>& fri_steps, bool is_zero_knowledge);

}  // namespace starkware

#endif  // STARKWARE_UTILS_INPUT_UTILS_H_
