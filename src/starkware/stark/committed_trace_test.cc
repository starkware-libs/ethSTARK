#include "starkware/stark/committed_trace.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/merkle/merkle_commitment_scheme.h"
#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/commitment_scheme/table_verifier_impl.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/stark/test_utils.h"
#include "starkware/stark/utils.h"

namespace starkware {
namespace {

template <typename FieldElementT>
void CommittedTraceBasicFlowTest(
    size_t trace_length, size_t n_cosets, size_t n_columns, size_t mask_size,
    bool eval_in_natural_order) {
  Prng prng;

  const EvaluationDomain evaluation_domain(trace_length, n_cosets);

  // Prover setup.
  Prng channel_prng;
  ProverChannel prover_channel(ProverChannel(channel_prng.Clone()));

  TableProverFactory<FieldElementT> table_prover_factory =
      GetTableProverFactory<FieldElementT>(&prover_channel);

  // Random trace.
  std::vector<std::vector<FieldElementT>> trace_columns;
  trace_columns.reserve(n_columns);
  for (size_t i = 0; i < n_columns; ++i) {
    trace_columns.emplace_back(prng.RandomFieldElementVector<FieldElementT>(trace_length));
  }

  // Commit.
  CommittedTraceProver<FieldElementT> committed_trace_prover(
      UseOwned(&evaluation_domain), n_columns, table_prover_factory);
  committed_trace_prover.Commit(
      TraceBase<FieldElementT>(std::move(trace_columns)), evaluation_domain.TraceDomain(),
      eval_in_natural_order);

  auto lde = committed_trace_prover.GetLde();

  // Test constants.
  EXPECT_EQ(committed_trace_prover.NumColumns(), n_columns);
  EXPECT_EQ(committed_trace_prover.GetLde()->NumColumns(), n_columns);

  // Random queries (coset_index, offset, column_index).
  const size_t n_queries = 12;
  ASSERT_RELEASE(n_queries >= 2, "Not enough queries for test.");

  std::vector<std::tuple<uint64_t, uint64_t, size_t>> queries;
  queries.reserve(n_queries);
  for (size_t i = 0; i < n_queries - 1; ++i) {
    queries.emplace_back(
        prng.UniformInt<uint64_t>(0, n_cosets - 1), prng.UniformInt<uint64_t>(0, trace_length - 1),
        prng.UniformInt<uint64_t>(0, n_columns - 1));
  }
  queries.push_back(queries[prng.UniformInt<size_t>(0, n_queries - 2)]);

  // Decommit.
  committed_trace_prover.DecommitQueries(queries);

  // Random mask.
  std::vector<std::pair<int64_t, uint64_t>> mask;
  for (size_t i = 0; i < mask_size; ++i) {
    mask.emplace_back(
        prng.UniformInt<int64_t>(0, trace_length - 1), prng.UniformInt<int64_t>(0, n_columns - 1));
  }

  // EvalMaskAtPoint.
  const ExtensionFieldElement point = ExtensionFieldElement::RandomElement(&prng);
  BaseFieldElement trace_gen = evaluation_domain.TraceGenerator();
  auto eval_mask_output = ExtensionFieldElement::UninitializedVector(mask.size());
  committed_trace_prover.EvalMaskAtPoint(mask, point, eval_mask_output);

  // Test EvalMaskAtPoint correctness.
  for (size_t i = 0; i < mask_size; ++i) {
    const auto& [mask_row, mask_col] = mask[i];
    const auto shifted_point = std::vector<ExtensionFieldElement>{point * Pow(trace_gen, mask_row)};
    auto expected_output = ExtensionFieldElement::UninitializedVector(1);
    lde->EvalAtPointsNotCached(mask_col, shifted_point, expected_output);
    ASSERT_EQ(expected_output[0], eval_mask_output[i]);
  }

  // Verifier setup.
  VerifierChannel verifier_channel(channel_prng.Clone(), prover_channel.GetProof());
  verifier_channel.SetExpectedAnnotations(prover_channel.GetAnnotations());
  TableVerifierFactory<FieldElementT> table_verifier_factory =
      [&verifier_channel](uint64_t n_rows, uint64_t n_columns) {
        return MakeTableVerifier<FieldElementT>(n_rows, n_columns, &verifier_channel);
      };

  CommittedTraceVerifier<FieldElementT> committed_trace_verifier(
      UseOwned(&evaluation_domain), n_columns, table_verifier_factory);

  // Read commitment.
  committed_trace_verifier.ReadCommitment();

  // Verify decommitment.
  std::vector<FieldElementT> results = committed_trace_verifier.VerifyDecommitment(queries);

  // Test results correctness.
  for (size_t query_index = 0; query_index < queries.size(); ++query_index) {
    const auto& [coset_index, offset, column_index] = queries[query_index];
    // Coset order is bit-reversed in CommittedTraceProver.
    const uint64_t coset_index_rev = BitReverse(coset_index, SafeLog2(n_cosets));
    // Compute expected value.
    const ExtensionFieldElement point(evaluation_domain.ElementByIndex(coset_index_rev, offset));
    const std::vector<ExtensionFieldElement> points{point};
    auto output = ExtensionFieldElement::UninitializedVector(1);
    lde->EvalAtPointsNotCached(column_index, points, output);
    const ExtensionFieldElement expected_value = output[0];
    const ExtensionFieldElement value(results[query_index]);
    EXPECT_EQ(value, expected_value);
  }
}

template <typename T>
class CommittedTraceTest : public ::testing::Test {};

using TestedFieldTypes = ::testing::Types<BaseFieldElement, ExtensionFieldElement>;
TYPED_TEST_CASE(CommittedTraceTest, TestedFieldTypes);

TYPED_TEST(CommittedTraceTest, Basic) {
  const size_t trace_length = 16;
  const size_t n_cosets = 8;
  const size_t n_columns = 7;
  const size_t mask_size = 12;

  CommittedTraceBasicFlowTest<TypeParam>(
      trace_length, n_cosets, n_columns, mask_size, /*eval_in_natural_order=*/true);
  CommittedTraceBasicFlowTest<TypeParam>(
      trace_length, n_cosets, n_columns, mask_size, /*eval_in_natural_order=*/false);
}

}  // namespace
}  // namespace starkware
