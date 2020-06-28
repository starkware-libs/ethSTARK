#include "starkware/stark/oods.h"

#include <algorithm>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/test_air/test_air.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/channel/prover_channel.h"
#include "starkware/channel/prover_channel_mock.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/channel/verifier_channel_mock.h"
#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/commitment_scheme/table_verifier_impl.h"
#include "starkware/composition_polynomial/composition_polynomial_mock.h"
#include "starkware/stark/committed_trace_mock.h"
#include "starkware/stark/composition_oracle.h"
#include "starkware/stark/test_utils.h"
#include "starkware/stark/utils.h"

namespace starkware {
namespace {

using testing::_;
using testing::BeginEndDistanceIs;
using testing::Return;
using testing::StrictMock;

/*
  Used to mock the output of EvalMaskAtPoint().
  Example use: EXPECT_CALL(..., EvalMaskAtPoint(...)).WillOnce(SetEvaluation(mock_evaluation)).
*/
ACTION_P(SetEvaluation, eval) {
  gsl::span<ExtensionFieldElement> outputs = arg2;
  ASSERT_RELEASE(outputs.size() == eval.size(), "Wrong evaluation size.");
  std::copy(eval.begin(), eval.end(), outputs.begin());
}

/*
  Tests that ProveOods sends exactly what we expect:
  * The evaluation of the mask of the original trace at z.
  * The evaluations of the composition trace columns at z.
*/
TEST(OutOfDomainSampling, ProveOods) {
  const size_t trace_length = 16;
  const size_t n_cosets = 4;
  const size_t mask_size = 7;
  const size_t n_columns = 11;
  const size_t n_breaks = 2;

  // Setup.
  Prng channel_prng;
  const ExtensionFieldElement expected_point =
      ProverChannel(channel_prng.Clone()).GetRandomFieldElementFromVerifier("");
  Prng prng;
  const EvaluationDomain evaluation_domain(trace_length, n_cosets);

  std::vector<bool> columns_seen(n_columns, false);
  std::vector<std::pair<int64_t, uint64_t>> mask;
  mask.reserve(mask_size);
  for (size_t i = 0; i < mask_size; ++i) {
    int64_t row_index = prng.UniformInt<int64_t>(0, n_cosets - 1);
    int64_t column_index = prng.UniformInt<int64_t>(0, n_columns - 1);
    columns_seen[column_index] = true;
    mask.emplace_back(row_index, column_index);
  }
  const size_t mask_unique_columns = std::count(columns_seen.begin(), columns_seen.end(), true);

  //  Generate random values to send as OODS values.
  const auto expected_mask_eval_values =
      prng.RandomFieldElementVector<ExtensionFieldElement>(mask_size);
  const auto expected_composition_trace_values =
      prng.RandomFieldElementVector<ExtensionFieldElement>(n_breaks);

  // Set mocks for the traces.
  StrictMock<CommittedTraceProverMock<BaseFieldElement>> original_trace;
  StrictMock<CommittedTraceProverMock<ExtensionFieldElement>> composition_trace;
  EXPECT_CALL(original_trace, NumColumns()).WillRepeatedly(Return(n_columns));
  EXPECT_CALL(composition_trace, NumColumns()).WillRepeatedly(Return(n_breaks));

  // Create composition oracle.
  ProverChannel prover_channel(ProverChannel(channel_prng.Clone()));
  CompositionOracleProver oracle(
      UseOwned(&evaluation_domain), UseOwned(&original_trace), nullptr, mask, nullptr, nullptr,
      &prover_channel);

  // Set a mock for original_trace.EvalMaskAtPoint() call.
  EXPECT_CALL(original_trace, EvalMaskAtPoint(_, expected_point, BeginEndDistanceIs(mask.size())))
      .WillOnce(SetEvaluation(expected_mask_eval_values));

  // Set a mock for composition_trace.EvalMaskAtPoint() call.
  EXPECT_CALL(
      composition_trace,
      EvalMaskAtPoint(_, Pow(expected_point, n_breaks), BeginEndDistanceIs(n_breaks)))
      .WillOnce(SetEvaluation(expected_composition_trace_values));

  // Test that ProveOods() returns the expected number of constraints:
  // mask_size constraints to verify the mask values.
  // mask_unique_columns constraints to verify the composition polynomial values.
  // n_breaks constraints to verify that the trace contains only BaseFieldElement.
  const auto boundary_constraints = oods::ProveOods(&prover_channel, oracle, composition_trace);
  EXPECT_EQ(boundary_constraints.size(), mask_size + mask_unique_columns + n_breaks);

  // Test that the prover sent_data is as expected.
  const auto sent_data = prover_channel.GetProof();
  // Simulate the interaction that is done with the channel in ProveOods().
  ProverChannel prover_channel_2(ProverChannel(channel_prng.Clone()));
  for (const auto& value : expected_mask_eval_values) {
    prover_channel_2.SendFieldElement(value);
  }
  for (const auto& value : expected_composition_trace_values) {
    prover_channel_2.SendFieldElement(value);
  }

  const auto expected_sent_data = prover_channel_2.GetProof();

  EXPECT_EQ(sent_data, expected_sent_data);
}

/*
  Tests that VerifyOods verifies the right formula, and returns the right value.
*/
TEST(OutOfDomainSampling, VerifyOods) {
  const size_t trace_length = 16;
  const size_t n_cosets = 4;
  const size_t mask_size = 7;
  const size_t n_columns = 11;
  const size_t n_breaks = 4;

  // Setup.
  Prng channel_prng;
  ProverChannel fake_prover_channel(channel_prng.Clone());
  const ExtensionFieldElement expected_point =
      fake_prover_channel.GetRandomFieldElementFromVerifier("");
  Prng prng;
  const EvaluationDomain evaluation_domain(trace_length, n_cosets);
  const Coset composition_eval_domain(trace_length * n_breaks, BaseFieldElement::One());

  std::vector<bool> columns_seen(n_columns, false);
  std::vector<std::pair<int64_t, uint64_t>> mask;
  mask.reserve(mask_size);
  for (size_t i = 0; i < mask_size; ++i) {
    int64_t row_index = prng.UniformInt<int64_t>(0, n_cosets - 1);
    int64_t column_index = prng.UniformInt<int64_t>(0, n_columns - 1);
    columns_seen[column_index] = true;
    mask.emplace_back(row_index, column_index);
  }
  size_t mask_unique_columns = std::count(columns_seen.begin(), columns_seen.end(), true);

  // Generate random values to receive as OODS values.
  const auto fake_mask_eval_values =
      prng.RandomFieldElementVector<ExtensionFieldElement>(mask_size);
  const auto fake_composition_trace_values =
      prng.RandomFieldElementVector<ExtensionFieldElement>(n_breaks);

  // Compute the value of the RHS of the OODS equation.
  ExtensionFieldElement expected_check_value =
      HornerEval(expected_point, fake_composition_trace_values);

  // Create a fake proof.
  for (const auto& value : fake_mask_eval_values) {
    fake_prover_channel.SendFieldElement(value);
  }
  for (const auto& value : fake_composition_trace_values) {
    fake_prover_channel.SendFieldElement(value);
  }

  VerifierChannel verifier_channel(channel_prng.Clone(), fake_prover_channel.GetProof());

  // Set mocks.
  StrictMock<CommittedTraceVerifierMock<BaseFieldElement>> original_trace;
  EXPECT_CALL(original_trace, NumColumns()).WillRepeatedly(Return(n_columns));

  StrictMock<CompositionPolynomialMock> composition_polynomial_mock;
  EXPECT_CALL(composition_polynomial_mock, GetDegreeBound())
      .WillRepeatedly(Return(trace_length * n_breaks));

  // Create composition oracle.
  CompositionOracleVerifier oracle(
      UseOwned(&evaluation_domain), UseOwned(&original_trace), nullptr, mask, nullptr,
      UseOwned(&composition_polynomial_mock), &verifier_channel);

  // Mocks returned value on the LHS of the OODS equation, to be the same as the RHS we computed
  // before.
  EXPECT_CALL(
      composition_polynomial_mock, EvalAtPoint(
                                       expected_point, gsl::make_span(fake_mask_eval_values),
                                       gsl::span<const ExtensionFieldElement>{}))
      .WillOnce(Return(expected_check_value));

  // Test that VerifyOods() returns the expected number of constraints:
  // mask_size constraints to verify the mask values.
  // mask_unique_columns constraints to verify the composition polynomial values.
  // n_breaks constraints to verify that the trace contains only BaseFieldElement.
  const auto boundary_constraints =
      oods::VerifyOods(evaluation_domain, &verifier_channel, oracle, composition_eval_domain);
  EXPECT_EQ(boundary_constraints.size(), mask_size + mask_unique_columns + n_breaks);
}

TEST(OutOfDomainSampling, EndToEnd) {
  Prng prng;
  const BaseFieldElement& secret = BaseFieldElement::RandomElement(&prng);
  const size_t trace_length = 16;
  const size_t res_claim_index = 12;
  const size_t n_cosets = 8;

  // Setup domain.
  Prng channel_prng;
  ProverChannel prover_channel(channel_prng.Clone());
  const EvaluationDomain evaluation_domain(trace_length, n_cosets);

  // Initialize dummy test air.
  const BaseFieldElement claimed_res =
      TestAir::PublicInputFromPrivateInput(secret, res_claim_index);
  const TestAir test_air(trace_length, res_claim_index, claimed_res);

  // Commit on trace.
  const size_t n_columns = test_air.NumColumns();
  const TableProverFactory<BaseFieldElement> base_table_prover_factory =
      GetTableProverFactory<BaseFieldElement>(&prover_channel);
  CommittedTraceProver<BaseFieldElement> trace_prover(
      UseOwned(&evaluation_domain), n_columns, base_table_prover_factory);
  trace_prover.Commit(
      TestAir::GetTrace(secret, trace_length, res_claim_index), evaluation_domain.TraceDomain(),
      true);

  // Create composition polynomial.
  const size_t num_random_coefficients_required = test_air.NumRandomCoefficients();
  const std::vector<ExtensionFieldElement> random_coefficients =
      prng.RandomFieldElementVector<ExtensionFieldElement>(num_random_coefficients_required);
  const std::unique_ptr<const CompositionPolynomial> composition_polynomial =
      test_air.CreateCompositionPolynomial(evaluation_domain.TraceGenerator(), random_coefficients);

  // Create OracleProver.
  const CompositionOracleProver oracle_prover(
      UseOwned(&evaluation_domain), UseOwned(&trace_prover), nullptr, test_air.GetMask(),
      UseOwned(&test_air), UseOwned(composition_polynomial.get()), &prover_channel);

  // Break composition polynomial.
  const size_t n_breaks = oracle_prover.ConstraintsDegreeBound();
  const Coset composition_eval_domain(trace_length * n_breaks, BaseFieldElement::Generator());
  const std::vector<ExtensionFieldElement> composition_polynomial_eval =
      oracle_prover.EvalComposition(1, oracle_prover.ConstraintsDegreeBound());
  const TableProverFactory<ExtensionFieldElement> extension_table_prover_factory =
      GetTableProverFactory<ExtensionFieldElement>(&prover_channel);
  CommittedTraceProver<ExtensionFieldElement> composition_prover(
      UseOwned(&evaluation_domain), n_breaks, extension_table_prover_factory);
  {
    auto&& [uncommitted_composition_trace, composition_trace_domain] =
        oods::BreakCompositionPolynomial(
            composition_polynomial_eval, n_breaks, composition_eval_domain);
    composition_prover.Commit(
        std::move(uncommitted_composition_trace), composition_trace_domain, false);
  }

  // ProveOods.
  const auto prover_boundary_constraints =
      oods::ProveOods(&prover_channel, oracle_prover, composition_prover);

  // Initialize verifier channel.
  VerifierChannel verifier_channel(channel_prng.Clone(), prover_channel.GetProof());

  // Create verifier trace.
  const TableVerifierFactory<BaseFieldElement> base_table_verifier_factory =
      [&verifier_channel](uint64_t n_rows, uint64_t n_columns) {
        return MakeTableVerifier<BaseFieldElement>(n_rows, n_columns, &verifier_channel);
      };
  CommittedTraceVerifier<BaseFieldElement> trace_verifier(
      UseOwned(&evaluation_domain), n_columns, base_table_verifier_factory);
  trace_verifier.ReadCommitment();

  // Create verifier committed trace.
  const TableVerifierFactory<ExtensionFieldElement> extension_table_verifier_factory =
      [&verifier_channel](uint64_t n_rows, uint64_t n_columns) {
        return MakeTableVerifier<ExtensionFieldElement>(n_rows, n_columns, &verifier_channel);
      };
  CommittedTraceVerifier<ExtensionFieldElement> composition_trace_verifier(
      UseOwned(&evaluation_domain), n_breaks, extension_table_verifier_factory);
  composition_trace_verifier.ReadCommitment();

  // Create verifier oracle.
  const CompositionOracleVerifier oracle_verifier(
      UseOwned(&evaluation_domain), UseOwned(&trace_verifier), nullptr, test_air.GetMask(),
      UseOwned(&test_air), UseOwned(composition_polynomial.get()), &verifier_channel);

  // VerifyOods.
  const auto verifier_boundary_constraints = oods::VerifyOods(
      evaluation_domain, &verifier_channel, oracle_verifier, composition_eval_domain);

  // Check that the prover and the verifier return the same constraints.
  EXPECT_EQ(prover_boundary_constraints, verifier_boundary_constraints);
}

}  // namespace
}  // namespace starkware
