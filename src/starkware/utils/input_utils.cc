#include <algorithm>

#include "starkware/math/math.h"
#include "starkware/utils/input_utils.h"
#include "starkware/utils/json_builder.h"

namespace starkware {

JsonValue GetProverConfigJson(size_t constraint_polynomial_task_size) {
  JsonBuilder output;

  output["constraint_polynomial_task_size"] = constraint_polynomial_task_size;

  return output.Build();
}

JsonValue GetParametersJson(
    uint64_t trace_length, uint64_t log_n_cosets, uint64_t security_bits,
    uint64_t proof_of_work_bits, const std::vector<size_t>& fri_steps, bool is_zero_knowledge) {
  JsonBuilder params;

  uint64_t log_degree_bound = SafeLog2(trace_length);
  uint64_t n_queries = DivCeil(security_bits - proof_of_work_bits, log_n_cosets);

  uint64_t spare_degree = log_degree_bound;
  for (const size_t fri_step : fri_steps) {
    params["stark"]["fri"]["fri_step_list"].Append(fri_step);
    spare_degree -= fri_step;
  }

  params["stark"]["enable_zero_knowledge"] = is_zero_knowledge;
  params["stark"]["log_n_cosets"] = log_n_cosets;
  params["stark"]["fri"]["last_layer_degree_bound"] = Pow2(spare_degree);
  params["stark"]["fri"]["n_queries"] = n_queries;
  params["stark"]["fri"]["proof_of_work_bits"] = proof_of_work_bits;

  return params.Build();
}

}  // namespace starkware
