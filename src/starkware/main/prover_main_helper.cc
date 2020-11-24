#include "starkware/main/prover_main_helper.h"

#include <utility>

#include "glog/logging.h"

#include "starkware/stark/stark.h"
#include "starkware/stark/utils.h"
#include "starkware/utils/json_builder.h"

namespace starkware {

namespace {

/*
  Converts the content of a string into a JSON array. Each line of the string is an array item.
*/
JsonValue StringToJsonArray(const std::string& string_with_newlines) {
  JsonBuilder output;
  output["array"] = JsonValue::EmptyArray();
  std::istringstream stream(string_with_newlines);
  std::string line;
  while (getline(stream, line)) {
    output["array"].Append(line);
  }
  auto json = output.Build();
  return json["array"];
}

/*
  Writes the prover's context into a file.
  The context includes all inputs, outputs and configuration of the prover.
*/
void SaveProverContext(
    const std::string& file_name, const JsonValue& public_input, const JsonValue& parameters,
    const std::string& proof, const std::string& annotations) {
  JsonBuilder output;

  output["public_input"] = public_input;
  output["proof_parameters"] = parameters;
  output["proof_hex"] = proof;
  if (!annotations.empty()) {
    output["annotations"] = StringToJsonArray(annotations);
  }
  output.Build().Write(file_name);
}

}  // namespace

std::vector<std::byte> ProverMainHelper(
    Statement* statement, const JsonValue& parameters, const JsonValue& stark_config_json,
    const JsonValue& public_input, const std::string& out_file_name, bool generate_annotations) {
  const Air& air = statement->GetAir(
      parameters["stark"]["enable_zero_knowledge"].AsBool(),
      parameters["stark"]["fri"]["n_queries"].AsSizeT());

  StarkProverConfig stark_config(StarkProverConfig::FromJson(stark_config_json));

  StarkParameters stark_params = StarkParameters::FromJson(parameters["stark"], UseOwned(&air));

  ProverChannel channel{Prng(statement->GetInitialHashChainSeed())};
  if (!generate_annotations) {
    channel.DisableAnnotations();
  }

  Prng salts_prng(statement->GetZeroKnowledgeHashChainSeed());
  // Serialization of "Salts".
  salts_prng.MixSeedWithBytes(MakeByteArray<0x53, 0x61, 0x6c, 0x74, 0x73>());

  TableProverFactory<BaseFieldElement> base_table_prover_factory =
      GetTableProverFactory<BaseFieldElement>(
          &channel, stark_params.is_zero_knowledge, &salts_prng);

  TableProverFactory<ExtensionFieldElement> extension_table_prover_factory =
      GetTableProverFactory<ExtensionFieldElement>(&channel);

  AnnotationScope scope(&channel, statement->GetName());
  StarkProver prover(
      UseOwned(&channel), UseOwned(&base_table_prover_factory),
      UseOwned(&extension_table_prover_factory), UseOwned(&stark_params), UseOwned(&stark_config));
  // Generate trace.
  ProfilingBlock profiling_block("Trace generation");

  Prng trace_prng(statement->GetZeroKnowledgeHashChainSeed());
  // Serialization of "Trace".
  trace_prng.MixSeedWithBytes(MakeByteArray<0x54, 0x72, 0x61, 0x63, 0x65>());
  Trace trace = statement->GetTrace(&trace_prng);
  if (stark_params.is_zero_knowledge) {
    trace.AddZeroKnowledgeExtraColumn(&trace_prng);
  }

  profiling_block.CloseBlock();
  prover.ProveStark(std::move(trace));

  LOG(INFO) << channel.GetStatistics().ToString();

  std::vector<std::byte> proof_bytes = channel.GetProof();

  if (!out_file_name.empty()) {
    std::ostringstream channel_output;
    if (generate_annotations) {
      // Write annotations and proof statistics.
      channel_output << channel;
    }
    SaveProverContext(
        out_file_name, public_input, parameters, BytesToHexString(proof_bytes, false),
        channel_output.str());
  }
  return proof_bytes;
}

}  // namespace starkware
