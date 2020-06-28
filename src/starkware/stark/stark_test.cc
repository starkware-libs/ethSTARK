#include "starkware/stark/stark.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/test_air/test_air.h"
#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/commitment_scheme_builder.h"
#include "starkware/commitment_scheme/merkle/merkle_commitment_scheme.h"
#include "starkware/commitment_scheme/packaging_commitment_scheme.h"
#include "starkware/commitment_scheme/table_prover_impl.h"
#include "starkware/commitment_scheme/table_verifier_impl.h"
#include "starkware/crypt_tools/blake2s_160.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/proof_system/proof_system.h"
#include "starkware/stark/utils.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {
namespace {

using testing::HasSubstr;

template <typename FieldElementT>
std::unique_ptr<TableProver<FieldElementT>> MakeTableProver(
    uint64_t n_segments, uint64_t n_rows_per_segment, size_t n_columns, ProverChannel* channel) {
  return GetTableProverFactory<FieldElementT>(channel)(n_segments, n_rows_per_segment, n_columns);
}

template <typename FieldElementT>
std::unique_ptr<TableVerifier<FieldElementT>> MakeTableVerifier(
    uint64_t n_rows, uint64_t n_columns, VerifierChannel* channel) {
  auto packaging_commitment_scheme =
      MakeCommitmentSchemeVerifier(n_columns * FieldElementT::SizeInBytes(), n_rows, channel);

  return std::make_unique<TableVerifierImpl<FieldElementT>>(
      n_columns, UseMovedValue(std::move(packaging_commitment_scheme)), channel);
}

std::vector<size_t> DrawFriStepsList(const size_t log_degree_bound, Prng* prng) {
  if (prng == nullptr) {
    return {3, 3, 1, 1};
  }
  std::vector<size_t> res;
  for (size_t curr_sum = 0; curr_sum < log_degree_bound;) {
    res.push_back(prng->UniformInt<size_t>(1, log_degree_bound - curr_sum));
    curr_sum += res.back();
  }
  ASSERT_RELEASE(
      Sum(res) == log_degree_bound, "The sum of all steps must be the log of the degree bound.");
  return res;
}

StarkParameters GenerateParameters(
    std::unique_ptr<Air> air, Prng* prng, size_t proof_of_work_bits = 15) {
  const size_t trace_length = air->TraceLength();
  uint64_t degree_bound = air->GetCompositionPolynomialDegreeBound() / trace_length;

  const auto log_n_cosets =
      (prng == nullptr ? 4 : prng->UniformInt<size_t>(Log2Ceil(degree_bound), 6));
  const size_t log_coset_size = Log2Ceil(trace_length);
  const size_t n_cosets = Pow2(log_n_cosets);
  const size_t log_evaluation_domain_size = log_coset_size + log_n_cosets;

  // FRI steps - hard-coded pattern for these tests.
  const size_t log_degree_bound = log_coset_size;
  const std::vector<size_t> fri_step_list = DrawFriStepsList(log_degree_bound, prng);

  // The coset offset used by FRI.
  const BaseFieldElement offset =
      (prng == nullptr ? BaseFieldElement::One() : BaseFieldElement::RandomElement(prng));

  // FRI parameters.
  FriParameters fri_params{
      /*fri_step_list=*/fri_step_list,
      /*last_layer_degree_bound=*/1,
      /*n_queries=*/30,
      /*domain=*/Coset(Pow2(log_evaluation_domain_size), offset),
      /*proof_of_work_bits=*/proof_of_work_bits,
  };

  return StarkParameters(
      n_cosets, trace_length, TakeOwnershipFrom(std::move(air)),
      UseMovedValue(std::move(fri_params)));
}

class StarkTest : public ::testing::Test {
 public:
  explicit StarkTest(bool use_random_values = true)
      : prng(use_random_values ? Prng() : Prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>())),
        channel_prng(use_random_values ? Prng() : Prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>())),
        stark_config(StarkProverConfig::Default()),
        prover_channel(ProverChannel(channel_prng.Clone())) {
    base_table_prover_factory =
        [this](uint64_t n_segments, uint64_t n_rows_per_segment, size_t n_columns) {
          return MakeTableProver<BaseFieldElement>(
              n_segments, n_rows_per_segment, n_columns, &prover_channel);
        };
    extension_table_prover_factory =
        [this](uint64_t n_segments, uint64_t n_rows_per_segment, size_t n_columns) {
          return MakeTableProver<ExtensionFieldElement>(
              n_segments, n_rows_per_segment, n_columns, &prover_channel);
        };
  }

  bool VerifyProof(
      const std::vector<std::byte>& proof,
      const std::optional<std::vector<std::string>>& prover_annotations = std::nullopt) {
    VerifierChannel verifier_channel(channel_prng.Clone(), proof);
    if (prover_annotations) {
      verifier_channel.SetExpectedAnnotations(*prover_annotations);
    }

    TableVerifierFactory<BaseFieldElement> base_table_verifier_factory =
        [&verifier_channel](uint64_t n_rows, uint64_t n_columns) {
          return MakeTableVerifier<BaseFieldElement>(n_rows, n_columns, &verifier_channel);
        };
    TableVerifierFactory<ExtensionFieldElement> extension_table_verifier_factory =
        [&verifier_channel](uint64_t n_rows, uint64_t n_columns) {
          return MakeTableVerifier<ExtensionFieldElement>(n_rows, n_columns, &verifier_channel);
        };
    StarkVerifier stark_verifier(
        UseOwned(&verifier_channel), UseOwned(&base_table_verifier_factory),
        UseOwned(&extension_table_verifier_factory), UseOwned(&GetStarkParams()));
    return FalseOnError([&]() { stark_verifier.VerifyStark(); });
  }

  virtual const StarkParameters& GetStarkParams() = 0;

  const uint64_t trace_length = 256;
  Prng prng;
  const Prng channel_prng;
  StarkProverConfig stark_config;

  ProverChannel prover_channel;
  TableProverFactory<BaseFieldElement> base_table_prover_factory;
  TableProverFactory<ExtensionFieldElement> extension_table_prover_factory;
};

class TestAirStarkTest : public StarkTest {
 public:
  explicit TestAirStarkTest(bool use_random_values = true)
      : StarkTest(use_random_values),
        secret(BaseFieldElement::RandomElement(&(this->prng))),
        claimed_res(TestAir::PublicInputFromPrivateInput(secret, res_claim_index)),
        stark_params(GenerateParameters(
            std::make_unique<TestAir>(this->trace_length, res_claim_index, claimed_res),
            use_random_values ? &(this->prng) : nullptr, 15)) {}

  const StarkParameters& GetStarkParams() override { return stark_params; }

  std::vector<std::byte> GenerateProof() { return GenerateProofWithAnnotations().first; }

  std::pair<std::vector<std::byte>, std::vector<std::string>> GenerateProofWithAnnotations() {
    auto air = TestAir(this->trace_length, res_claim_index, claimed_res);
    StarkProver stark_prover(
        UseOwned(&(this->prover_channel)), UseOwned(&(this->base_table_prover_factory)),
        UseOwned(&(this->extension_table_prover_factory)), UseOwned(&GetStarkParams()),
        UseOwned(&(this->stark_config)));

    stark_prover.ProveStark(TestAir::GetTrace(secret, trace_length, res_claim_index));
    return {this->prover_channel.GetProof(), this->prover_channel.GetAnnotations()};
  }

  void SetAir(TestAir air) { stark_params.air = UseMovedValue<TestAir>(std::move(air)); }

  const uint64_t res_claim_index = 251;
  const BaseFieldElement secret;
  const BaseFieldElement claimed_res;
  StarkParameters stark_params;
};

TEST_F(TestAirStarkTest, Correctness) {
  // Generate proof.
  const auto proof_annotations_pair = this->GenerateProofWithAnnotations();

  // Verify proof.
  EXPECT_TRUE(this->VerifyProof(proof_annotations_pair.first, proof_annotations_pair.second));
}

// Derive from StarkTest to call the constructor with use_random_values=false.

class StarkTestConstSeed : public TestAirStarkTest {
 public:
  StarkTestConstSeed() : TestAirStarkTest(/*use_random_values*/ false) {}
};

TEST_F(TestAirStarkTest, StarkWithFriProverBadTrace) {
  // Create a trace and corrupt it at exactly one location by incrementing it by one.
  auto bad_column = this->prng.template UniformInt<size_t>(0, 1);
  auto bad_index = this->prng.template UniformInt<size_t>(0, this->res_claim_index - 1);
  Trace trace = TestAir::GetTrace(this->secret, this->trace_length, this->res_claim_index);
  trace.SetTraceElementForTesting(
      bad_column, bad_index, trace.GetColumn(bad_column).at(bad_index) + BaseFieldElement::One());

  // Send the corrupted trace to a STARK prover.
  StarkProver stark_prover(
      UseOwned(&this->prover_channel), UseOwned(&this->base_table_prover_factory),
      UseOwned(&this->extension_table_prover_factory), UseOwned(&(this->GetStarkParams())),
      UseOwned(&this->stark_config));

  stark_prover.ProveStark(std::move(trace));
  EXPECT_FALSE(this->VerifyProof(this->prover_channel.GetProof()));
}

TEST_F(TestAirStarkTest, StarkWithFriProverPublicInputInconsistentWithWitness) {
  TestAir air(this->trace_length, this->res_claim_index, this->claimed_res);

  // Bad claimed element.
  this->stark_params = GenerateParameters(
      std::make_unique<TestAir>(
          this->trace_length, this->res_claim_index, this->claimed_res + BaseFieldElement::One()),
      &(this->prng));

  StarkProver stark_prover(
      UseOwned(&this->prover_channel), UseOwned(&this->base_table_prover_factory),
      UseOwned(&this->extension_table_prover_factory), UseOwned(&this->stark_params),
      UseOwned(&this->stark_config));

  stark_prover.ProveStark(
      TestAir::GetTrace(this->secret, this->trace_length, this->res_claim_index));
  EXPECT_FALSE(this->VerifyProof(this->prover_channel.GetProof()));
}

TEST_F(TestAirStarkTest, StarkWithFriProverWrongNumberOfLayers) {
  this->GetStarkParams().fri_params->fri_step_list = std::vector<size_t>(5, 1);
  EXPECT_ASSERT(this->GenerateProof(), HasSubstr("FRI parameters do not match"));
}

TEST_F(TestAirStarkTest, ChangeProofContent) {
  // Generate proof.
  const std::vector<std::byte> proof = this->GenerateProof();

  // Modify proof.
  std::vector<std::byte> modified_proof = proof;
  modified_proof[this->prng.template UniformInt<size_t>(0, modified_proof.size() - 1)] ^=
      std::byte(1);

  // Verify proof.
  EXPECT_FALSE(this->VerifyProof(modified_proof));
}

TEST_F(TestAirStarkTest, ShortenProof) {
  // Generate proof.
  const std::vector<std::byte> proof = this->GenerateProof();

  // Modify proof.
  std::vector<std::byte> modified_proof = proof;
  modified_proof.pop_back();

  // Verify proof.
  EXPECT_FALSE(this->VerifyProof(modified_proof));
}

TEST_F(TestAirStarkTest, ChangeAir) {
  // Generate proof.
  const std::vector<std::byte> proof = this->GenerateProof();

  // Modify AIR.
  this->SetAir(TestAir(
      this->trace_length, this->res_claim_index, this->claimed_res + BaseFieldElement::One()));

  // Verify proof.
  EXPECT_FALSE(this->VerifyProof(proof));
}

}  // namespace
}  // namespace starkware
