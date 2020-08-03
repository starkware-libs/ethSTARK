#ifndef STARKWARE_FRI_FRI_PARAMETERS_H_
#define STARKWARE_FRI_FRI_PARAMETERS_H_

#include <vector>

#include "starkware/algebra/domains/coset.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/json.h"

namespace starkware {

struct FriParameters {
  /*
    A list of fri_step_i (one per FRI layer). FRI reduction in the i-th layer will be 2^fri_step_i
    and the total reduction factor will be $2^{\sum_i fri_step_i}$. The size of fri_step_list is
    the number of FRI layers.

    For example, if fri_step_0 = 3, the second layer will be of size N/8 (where N is the size of the
    first layer). It means that the two merkle trees for layers of sizes N/2 and N/4 will be
    skipped. On the other hand, it means that each coset in the first layer is of size 8 instead
    of 2.

    For a simple (not optimal) FRI usage, take fri_step_list = {1, 1, ..., 1}.
  */
  std::vector<size_t> fri_step_list;

  /*
    In the original FRI protocol, one has to reduce the degree from N to 1 by using a total of
    log2(N) fri steps (sum of fri_step_list = log2(N)). This has the disadvantage that although the
    last layers are small, they still require Merkle authentication paths which are non-negligible.

    In our implementation, we reduce the degree from N to R (last_layer_degree_bound) for a
    relatively small R using log2(N/R) fri steps. To do it we send the R coefficients of the last
    FRI layer instead of continuing with additional FRI layers.

    To reduce proof-length, it is always better to pick last_layer_degree_bound > 1.
  */
  uint64_t last_layer_degree_bound;

  /*
    Number of queries for the FRI query phase.
  */
  size_t n_queries = 0;

  /*
    The evaluation domain.
  */
  Coset domain;

  /*
    If greater than 0, used to apply proof of work right before randomizing the FRI queries. Since
    the probability to draw bad queries is relatively high (at least 1/blowup for each query), while
    the probability to draw bad x^(0) values is ~1/|F|, the queries are more vulnerable to
    enumeration.
  */
  size_t proof_of_work_bits;

  /*
    Creates an instance from a given JSON representation, log(trace length) and log(number
    of cosets in the evaluation domain).
  */
  static FriParameters FromJson(
      const JsonValue& json, size_t log_trace_length, size_t log_n_cosets);

  /*
    Returns a coset representing the domain of the FRI layer at a given index.
    If the original domain is of size 2^N, then the returned coset is of size 2^(N-idx).
  */
  Coset GetCosetForLayer(size_t idx) const;

  /*
    Returns the size of the domain at a given FRI layer.
    For a domain of size 2^N, there are N layers and the i-th layer size is 2^(N-i).
    Note that some of the layers may be skipped (see fri_step_list).
    idx refers to the number of the layer before layer-skipping.
  */
  size_t GetLayerDomainSize(size_t idx) const {
    ASSERT_RELEASE(idx <= SafeLog2(domain.Size()), "Invalid layer index.");
    return SafeDiv(domain.Size(), Pow2(idx));
  }
};

inline FriParameters FriParameters::FromJson(
    const JsonValue& json, size_t log_trace_length, size_t log_n_cosets) {
  const std::vector<size_t> fri_step_list = json["fri_step_list"].AsSizeTVector();
  ASSERT_RELEASE(!fri_step_list.empty(), "fri_step_list must not be empty.");
  size_t cumulative_fri_step = 0;
  for (size_t i = 0; i < fri_step_list.size(); ++i) {
    ASSERT_RELEASE(
        fri_step_list.at(i) > 0 || i == 0, "fri_step_list at each layer must be at least 1.");
    ASSERT_RELEASE(
        fri_step_list.at(i) <= 10, "fri_step_list at each layer cannot be greater than 10.");
    cumulative_fri_step += fri_step_list.at(i);
  }
  ASSERT_RELEASE(
      cumulative_fri_step <= log_trace_length,
      "FRI total reduction cannot be greater than the trace length.");

  const uint64_t last_layer_degree_bound = json["last_layer_degree_bound"].AsSizeT();
  ASSERT_RELEASE(
      IsPowerOfTwo(last_layer_degree_bound), "last_layer_degree_bound must be a power of two.");
  ASSERT_RELEASE(
      last_layer_degree_bound > 0 && last_layer_degree_bound <= 16384,
      "Last layer degree bound must be in the range [1, 2^14].");
  ASSERT_RELEASE(
      SafeLog2(last_layer_degree_bound) + cumulative_fri_step == log_trace_length,
      "last_layer_degree_bound (" + std::to_string(last_layer_degree_bound) +
          ") and FRI total reduction (" + std::to_string(Pow2(cumulative_fri_step)) +
          ") do not match the trace length (" + std::to_string(Pow2(log_trace_length)) + ").");

  const size_t n_queries = json["n_queries"].AsSizeT();
  ASSERT_RELEASE(n_queries > 0 && n_queries <= 256, "n_queries must be in the range [1, 256].");

  const size_t proof_of_work_bits = json["proof_of_work_bits"].AsSizeT();
  ASSERT_RELEASE(
      proof_of_work_bits >= 0 && proof_of_work_bits <= 50,
      "proof_of_work_bits must be in the range [0, 50].");

  const Coset evaluation_domain(Pow2(log_trace_length + log_n_cosets), BaseFieldElement::One());

  return {fri_step_list, last_layer_degree_bound, n_queries, evaluation_domain, proof_of_work_bits};
}

/*
  Same as FriParameters::GetCosetForLayer(idx), for an arbitrary domain.
*/
inline Coset GetCosetForFriLayer(const Coset& layer_coset, size_t idx) {
  ASSERT_RELEASE(idx <= SafeLog2(layer_coset.Size()), "Invalid layer index.");
  return Coset(SafeDiv(layer_coset.Size(), Pow2(idx)), Pow(layer_coset.Offset(), Pow2(idx)));
}

inline Coset FriParameters::GetCosetForLayer(size_t idx) const {
  return GetCosetForFriLayer(domain, idx);
}

}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_PARAMETERS_H_
