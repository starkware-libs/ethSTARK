#include "benchmark/benchmark.h"
#include "gflags/gflags.h"

#include "starkware/error_handling/error_handling.h"
#include "starkware/main/prover_main_helper.h"
#include "starkware/main/verifier_main_helper.h"
#include "starkware/math/math.h"
#include "starkware/statement/ziggy/ziggy_statement.h"
#include "starkware/utils/input_utils.h"
#include "starkware/utils/json_builder.h"
#include "starkware/utils/to_from_string.h"

namespace starkware {
namespace {

static void ZiggyVerifierBenchmark(benchmark::State& state) {  // NOLINT
  static Prng prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  const size_t blowup = state.range(0);

  JsonBuilder private_input;
  ZiggyStatement::PrivateKeyT private_key;
  prng.GetRandomBytes(private_key);
  private_input["private_key"] = BytesToHexString(private_key);

  JsonBuilder public_input;
  public_input["message"] = "Hello World!";
  for (size_t i = 0; i < ZiggyAir::kPublicKeySize; ++i) {
    public_input["public_key"].Append("0x0");
  }
  ZiggyStatement statement(public_input.Build(), private_input.Build());
  statement.FixPublicInput();

  const JsonValue stark_config = GetProverConfigJson(64);

  size_t security_bits = 122;
  size_t proof_of_work_bits = 8;
  size_t n_queries = DivCeil(security_bits - proof_of_work_bits, blowup);
  auto trace_length = statement.GetAir(/*is_zero_knowledge=*/true, n_queries).TraceLength();
  const JsonValue parameters = GetParametersJson(
      /*trace_length=*/trace_length, /*log_n_cosets=*/blowup, /*security_bits=*/security_bits,
      /*proof_of_work_bits=*/proof_of_work_bits, /*fri_steps=*/{0}, /*is_zero_knowledge=*/true);

  // Generate a STARK proof for the benchmark.
  std::vector<std::byte> proof_data = ProverMainHelper(
      &statement, parameters, stark_config, JsonValue::FromJsonCppValue(Json::Value()));

  // NOLINTNEXTLINE: Suppressing warnings for unused variable '_'.
  for (auto _ : state) {
    VerifierMainHelper(&statement, proof_data, parameters, /*annotation_file_name=*/"");
  }
}

// NOLINTNEXTLINE: cppcoreguidelines-owning-memory.
BENCHMARK(ZiggyVerifierBenchmark)->DenseRange(2, 10, 1);

}  // namespace
}  // namespace starkware
