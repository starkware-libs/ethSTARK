#include "benchmark/benchmark.h"
#include "gflags/gflags.h"

#include "starkware/main/prover_main_helper.h"
#include "starkware/math/math.h"
#include "starkware/statement/rescue/rescue_statement.h"
#include "starkware/utils/input_utils.h"
#include "starkware/utils/json_builder.h"

/*
  The constant kChainLength is a number that's very close to 3 * 2**15.
  Given chain_length, the actual trace length should hold:
  actual_trace_length >= 32 * chain_length / 3  (chain_length is expected to be divisible by 3).
  In practice, the trace length we produce may exceed this value since we pad the trace so that its
  length is rounded up to the nearest power of 2. Therefore different chain_length values may
  yield the same trace length.
  For example, for a trace length of 2**20, chain_length may be up to 98304 = 3 * 2**15, but if
  chain_length = 98307 then the trace must be padded to fit a length of 2**21.
*/
static const uint64_t kChainLength = 98001;

namespace starkware {
namespace {

/*
  Generates a random witness for the benchmark.
*/
JsonValue GetPrivateInput(size_t chain_length, Prng* prng) {
  JsonBuilder private_input;
  for (size_t i = 0; i < chain_length + 1; ++i) {
    Json::Value value(Json::arrayValue);
    for (size_t j = 0; j < 4; ++j) {
      value.append(BaseFieldElement::RandomElement(prng).ToString());
    }
    private_input["witness"].Append(value);
  }
  return private_input.Build();
}

static void RescueProverBenchmark(benchmark::State& state) {  // NOLINT
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

  // NOLINTNEXTLINE: Suppressing warnings for unused variable '_'.
  for (auto _ : state) {
    ProverMainHelper(
        &statement, parameters, stark_config, JsonValue::FromJsonCppValue(Json::Value()));
  }
}

// NOLINTNEXTLINE: cppcoreguidelines-owning-memory.
BENCHMARK(RescueProverBenchmark)->DenseRange(2, 4, 1);

}  // namespace
}  // namespace starkware
