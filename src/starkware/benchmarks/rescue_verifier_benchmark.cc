#include "benchmark/benchmark.h"
#include "gflags/gflags.h"

#include "starkware/error_handling/error_handling.h"
#include "starkware/main/prover_main_helper.h"
#include "starkware/main/verifier_main_helper.h"
#include "starkware/math/math.h"
#include "starkware/statement/rescue/rescue_statement.h"
#include "starkware/utils/input_utils.h"
#include "starkware/utils/json_builder.h"

/*
  For comprehensive documentation see rescue_prover_benchmark.cc.
*/
static const uint64_t kChainLength = 98001;

namespace starkware {
namespace {

/*
  Generates a random witness for the benchmark.
*/
JsonValue GetPrivateInput(size_t chain_length, Prng* prng) {
  JsonBuilder private_input;
  for (size_t i = 0; i < chain_length + 1; i++) {
    Json::Value value(Json::arrayValue);
    for (size_t j = 0; j < 4; j++) {
      value.append(BaseFieldElement::RandomElement(prng).ToString());
    }
    private_input["witness"].Append(value);
  }
  return private_input.Build();
}

static void RescueVerifierBenchmark(benchmark::State& state) {  // NOLINT
  static Prng prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  const size_t blowup = state.range(0);

  const JsonValue private_input = GetPrivateInput(kChainLength, &prng);
  const JsonValue public_input =
      RescueStatement::GetPublicInputJsonValueFromPrivateInput(private_input);
  const JsonValue stark_config = GetProverConfigJson();

  RescueStatement statement(public_input, private_input);

  size_t security_bits = 128;
  size_t proof_of_work_bits = 18;
  size_t n_queries = DivCeil(security_bits - proof_of_work_bits, blowup);
  auto trace_length = statement.GetAir(/*is_zero_knowledge=*/false, n_queries).TraceLength();
  const JsonValue parameters = GetParametersJson(
      /*trace_length=*/trace_length, /*log_n_cosets=*/blowup, /*security_bits=*/security_bits,
      /*proof_of_work_bits=*/proof_of_work_bits, /*fri_steps=*/{1, 3, 3, 3, 3},
      /*is_zero_knowledge=*/false);

  // Generate a STARK proof for the benchmark.
  std::vector<std::byte> proof_data = ProverMainHelper(
      &statement, parameters, stark_config, JsonValue::FromJsonCppValue(Json::Value()));

  // NOLINTNEXTLINE: Suppressing warnings for unused variable '_'.
  for (auto _ : state) {
    VerifierMainHelper(&statement, proof_data, parameters, /*annotation_file_name=*/"");
  }
}

// NOLINTNEXTLINE: cppcoreguidelines-owning-memory.
BENCHMARK(RescueVerifierBenchmark)->DenseRange(2, 4, 1);

}  // namespace
}  // namespace starkware
