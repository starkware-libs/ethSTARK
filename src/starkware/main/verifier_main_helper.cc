#include "starkware/main/verifier_main_helper.h"

#include <fstream>
#include <memory>
#include <utility>

#include "glog/logging.h"

#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/commitment_scheme_builder.h"
#include "starkware/commitment_scheme/table_verifier_impl.h"
#include "starkware/proof_system/proof_system.h"
#include "starkware/randomness/prng.h"
#include "starkware/stark/stark.h"

namespace starkware {

bool VerifierMainHelper(
    Statement* statement, const std::vector<std::byte>& proof, const JsonValue& parameters,
    const std::string& annotation_file_name) {
  try {
    const Air& air = statement->GetAir(
        parameters["stark"]["enable_zero_knowledge"].AsBool(),
        parameters["stark"]["fri"]["n_queries"].AsSizeT());

    const StarkParameters stark_params =
        StarkParameters::FromJson(parameters["stark"], UseOwned(&air));

    VerifierChannel channel(Prng(statement->GetInitialHashChainSeed()), proof);
    if (annotation_file_name.empty()) {
      channel.DisableAnnotations();
    }

    TableVerifierFactory<BaseFieldElement> base_table_verifier_factory =
        [&channel, &stark_params](uint64_t n_rows, size_t n_columns) {
          auto packaging_commitment_scheme = MakeCommitmentSchemeVerifier(
              n_columns * BaseFieldElement::SizeInBytes(), n_rows, &channel,
              /*with_salt=*/stark_params.is_zero_knowledge);

          return std::make_unique<TableVerifierImpl<BaseFieldElement>>(
              n_columns, TakeOwnershipFrom(std::move(packaging_commitment_scheme)), &channel);
        };

    TableVerifierFactory<ExtensionFieldElement> extension_table_verifier_factory =
        [&channel](uint64_t n_rows, size_t n_columns) {
          auto packaging_commitment_scheme = MakeCommitmentSchemeVerifier(
              n_columns * ExtensionFieldElement::SizeInBytes(), n_rows, &channel);

          return std::make_unique<TableVerifierImpl<ExtensionFieldElement>>(
              n_columns, TakeOwnershipFrom(std::move(packaging_commitment_scheme)), &channel);
        };

    AnnotationScope scope(&channel, statement->GetName());
    // Create a StarkVerifier and verify the proof.
    StarkVerifier stark_verifier(
        UseOwned(&channel), UseOwned(&base_table_verifier_factory),
        UseOwned(&extension_table_verifier_factory), UseOwned(&stark_params));

    const bool result = FalseOnError([&]() { stark_verifier.VerifyStark(); });

    if (!annotation_file_name.empty()) {
      std::ofstream annotation_file(annotation_file_name);
      annotation_file << channel;
    }

    return result;
  } catch (const StarkwareException& e) {
    LOG(ERROR) << e.what();
    return false;
  }
}

}  // namespace starkware
