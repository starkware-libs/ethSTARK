#include "starkware/stark/composition_oracle.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/channel/prover_channel_mock.h"
#include "starkware/channel/verifier_channel_mock.h"
#include "starkware/composition_polynomial/composition_polynomial_mock.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/stark/committed_trace_mock.h"

namespace starkware {
namespace {

using testing::_;
using testing::AllOf;
using testing::BeginEndDistanceIs;
using testing::Each;
using testing::HasSubstr;
using testing::Invoke;
using testing::Property;
using testing::Return;
using testing::StrictMock;

/*
  Helper class for CompositionOracleProver tests. Holds parameters for test, and common setup code.
*/
class CompositionOracleProverTester {
 public:
  CompositionOracleProverTester(
      size_t trace_length, size_t n_cosets, size_t n_columns, bool with_composition_trace)
      : trace_length(trace_length),
        n_cosets(n_cosets),
        n_columns(n_columns),
        with_composition_trace(with_composition_trace),
        n_traces(with_composition_trace ? 2 : 1),
        evaluation_domain(trace_length, n_cosets) {
    // Create trace mocks.
    auto trace_mock = std::make_unique<StrictMock<CommittedTraceProverMock<BaseFieldElement>>>();
    EXPECT_CALL(*trace_mock, NumColumns()).WillRepeatedly(Return(n_columns));
    trace_ptr = TakeOwnershipFrom(std::move(trace_mock));

    if (with_composition_trace) {
      auto composition_trace_mock =
          std::make_unique<StrictMock<CommittedTraceProverMock<ExtensionFieldElement>>>();
      EXPECT_CALL(*composition_trace_mock, NumColumns()).WillRepeatedly(Return(n_columns));

      composition_trace_ptr = TakeOwnershipFrom(std::move(composition_trace_mock));
    }
  }

  void TestEvalComposition(size_t degree_bound);
  void TestDecommitQueries();
  void TestInvalidMask();

  size_t trace_length;
  size_t n_cosets;
  size_t n_columns;
  bool with_composition_trace;
  size_t n_traces;

  EvaluationDomain evaluation_domain;
  ProverChannelMock channel;

  MaybeOwnedPtr<CommittedTraceProverMock<BaseFieldElement>> trace_ptr;
  MaybeOwnedPtr<CommittedTraceProverMock<ExtensionFieldElement>> composition_trace_ptr;
};

/*
  Tests CompositionOracleProverTester::EvalComposition(). Feeds random traces to
  CompositionOracleProverTester, and checks that
  CompositionPolynomial::EvalOnCosetBitReversedOutput() is called with parameters of correct sizes.
*/
void CompositionOracleProverTester::TestEvalComposition(size_t degree_bound) {
  const size_t task_size = 32;
  std::unique_ptr<CachedLdeManager<BaseFieldElement>> trace_lde;
  std::unique_ptr<CachedLdeManager<ExtensionFieldElement>> composition_trace_lde;

  // Setup.
  Prng prng;

  const size_t n_cosets = evaluation_domain.NumCosets();
  std::vector<BaseFieldElement> coset_offsets_bit_reversed;
  coset_offsets_bit_reversed.reserve(n_cosets);
  const size_t log_cosets = SafeLog2(n_cosets);
  for (uint64_t i = 0; i < n_cosets; ++i) {
    coset_offsets_bit_reversed.emplace_back(
        evaluation_domain.CosetOffsets()[BitReverse(i, log_cosets)]);
  }

  // LDE random traces.
  // uninitialized_element is used as a workaround to infer the underlying FieldElement type in the
  // commited trace.
  auto lde_random_trace = [&](const auto& trace, const auto& uninitialized_element) {
    std::vector<BaseFieldElement> coset_offsets_bit_reversed_for_lde(coset_offsets_bit_reversed);
    using FieldElementT = std::decay_t<decltype(uninitialized_element)>;
    std::unique_ptr<LdeManager<FieldElementT>> lde_manager =
        MakeLdeManager<FieldElementT>(evaluation_domain.TraceDomain());
    auto cached_lde_manager = std::make_unique<CachedLdeManager<FieldElementT>>(
        TakeOwnershipFrom(std::move(lde_manager)), std::move(coset_offsets_bit_reversed_for_lde));
    for (size_t column_i = 0; column_i < n_columns; ++column_i) {
      cached_lde_manager->AddEvaluation(prng.RandomFieldElementVector<FieldElementT>(trace_length));
    }
    cached_lde_manager->FinalizeAdding();

    EXPECT_CALL(*trace, GetLde()).WillRepeatedly(Return(cached_lde_manager.get()));
    return cached_lde_manager;
  };
  trace_lde = lde_random_trace(trace_ptr, BaseFieldElement::Uninitialized());
  if (with_composition_trace) {
    composition_trace_lde =
        lde_random_trace(composition_trace_ptr, ExtensionFieldElement::Uninitialized());
  }

  // Composition polynomial mock.
  StrictMock<CompositionPolynomialMock> composition_polynomial;
  EXPECT_CALL(composition_polynomial, GetDegreeBound())
      .WillRepeatedly(Return(degree_bound * trace_length));
  for (uint64_t coset_index = 0; coset_index < degree_bound; coset_index++) {
    const BaseFieldElement coset_offset = coset_offsets_bit_reversed[coset_index];
    // Test that each coset is computed with the correct offset and the correct sizes of arguments.
    EXPECT_CALL(
        composition_polynomial,
        EvalOnCosetBitReversedOutput(
            coset_offset,
            AllOf(BeginEndDistanceIs(n_columns), Each(BeginEndDistanceIs(trace_length))),
            AllOf(
                BeginEndDistanceIs(with_composition_trace ? n_columns : 0),
                Each(BeginEndDistanceIs(trace_length))),
            Property(&gsl::span<ExtensionFieldElement>::size, trace_length), task_size));
  }

  // Create CompositionOracleProver.
  std::vector<std::pair<int64_t, uint64_t>> mask;

  CompositionOracleProver oracle_prover(
      UseOwned(&evaluation_domain), std::move(trace_ptr), std::move(composition_trace_ptr), mask,
      nullptr, UseOwned(&composition_polynomial), &channel);

  std::vector<ExtensionFieldElement> result =
      oracle_prover.EvalComposition(task_size, degree_bound);
  EXPECT_EQ(result.size(), degree_bound * trace_length);
}

TEST(CompositionOracleProver, EvalComposition) {
  CompositionOracleProverTester(16, 4, 7, true).TestEvalComposition(2);

  // Test a single coset.
  CompositionOracleProverTester(16, 1, 7, true).TestEvalComposition(1);

  // Test a single column.
  CompositionOracleProverTester(16, 4, 1, false).TestEvalComposition(2);

  // Test a single trace.
  CompositionOracleProverTester(16, 4, 7, false).TestEvalComposition(2);

  // Test degree 1.
  CompositionOracleProverTester(16, 4, 7, true).TestEvalComposition(1);

  // Test no cosets.
  EXPECT_ASSERT(
      CompositionOracleProverTester(16, 0, 7, true).TestEvalComposition(2),
      HasSubstr("must be a power of 2"));

  // Test degree 0.
  CompositionOracleProverTester(16, 4, 7, true).TestEvalComposition(0);
}

/*
  Tests CompositionPolynomialMock::DecommmitQueries(). Checks that DecommitQueries() is called with
  parameters of the correct size.
*/
void CompositionOracleProverTester::TestDecommitQueries() {
  Prng prng;

  // Composition polynomial mock.
  StrictMock<CompositionPolynomialMock> composition_polynomial;

  // Create CompositionOracleProver.
  std::vector<std::pair<int64_t, uint64_t>> mask;
  auto create_mask = [&](size_t trace_i, const auto& trace) {
    const size_t masks_items_in_trace = prng.UniformInt<size_t>(1, 5);
    const size_t column_offset = n_columns * trace_i;
    for (size_t i = 0; i < masks_items_in_trace; ++i) {
      mask.emplace_back(i, prng.UniformInt<size_t>(0, n_columns - 1) + column_offset);
    }

    EXPECT_CALL(
        *trace,
        DecommitQueries(Property(
            &gsl::span<const std::tuple<uint64_t, uint64_t, size_t>>::size, masks_items_in_trace)));
  };
  create_mask(0, trace_ptr);
  if (with_composition_trace) {
    create_mask(1, composition_trace_ptr);
  }

  CompositionOracleProver oracle_prover(
      UseOwned(&evaluation_domain), std::move(trace_ptr), std::move(composition_trace_ptr), mask,
      nullptr, UseOwned(&composition_polynomial), &channel);

  // Check a single query.
  oracle_prover.DecommitQueries({{prng.UniformInt<uint64_t>(0, n_cosets - 1),
                                  prng.UniformInt<uint64_t>(0, n_traces * n_columns - 1)}});
}

TEST(CompositionOracleProver, DecommitQueries) {
  CompositionOracleProverTester(16, 4, 7, true).TestDecommitQueries();

  // Test a single coset.
  CompositionOracleProverTester(16, 1, 7, true).TestDecommitQueries();

  // Test a single column.
  CompositionOracleProverTester(16, 4, 1, false).TestDecommitQueries();

  // Test a single trace.
  CompositionOracleProverTester(16, 4, 7, false).TestDecommitQueries();

  // Test no cosets.
  EXPECT_ASSERT(
      CompositionOracleProverTester(16, 0, 7, true).TestDecommitQueries(),
      HasSubstr("must be a power of 2"));
}

void CompositionOracleProverTester::TestInvalidMask() {
  Prng prng;

  // Composition polynomial mock.
  StrictMock<CompositionPolynomialMock> composition_polynomial;

  // Create CompositionOracleProver.
  std::vector<std::pair<int64_t, uint64_t>> mask;
  mask.reserve(n_traces * n_columns + 1);
  // Last column is out of range.
  for (size_t i = 0; i <= n_traces * n_columns; ++i) {
    mask.emplace_back(0, i);
  }

  EXPECT_ASSERT(
      CompositionOracleProver(
          UseOwned(&evaluation_domain), std::move(trace_ptr), nullptr, mask, nullptr,
          UseOwned(&composition_polynomial), &channel),
      HasSubstr("out of range"));
}

TEST(CompositionOracleProver, InvalidMask) {
  CompositionOracleProverTester(16, 4, 7, true).TestInvalidMask();
}

void VerifierTestVerifyDecommitment(
    size_t trace_length, size_t n_cosets, size_t n_columns, bool with_composition_trace,
    size_t n_queries) {
  ASSERT_RELEASE(n_queries >= 2, "Invalid number of queries for test");

  Prng prng;

  // Composition polynomial mock.
  StrictMock<CompositionPolynomialMock> composition_polynomial;

  // Create traces.
  const EvaluationDomain evaluation_domain(trace_length, n_cosets);

  // Create traces mocks.
  std::unique_ptr<StrictMock<CommittedTraceVerifierMock<BaseFieldElement>>> trace_ptr;
  {
    auto trace = std::make_unique<StrictMock<CommittedTraceVerifierMock<BaseFieldElement>>>();
    EXPECT_CALL(*trace, NumColumns()).WillRepeatedly(Return(n_columns));
    trace_ptr = std::move(trace);
  }

  std::unique_ptr<StrictMock<CommittedTraceVerifierMock<ExtensionFieldElement>>>
      composition_trace_ptr;
  if (with_composition_trace) {
    auto composition_trace =
        std::make_unique<StrictMock<CommittedTraceVerifierMock<ExtensionFieldElement>>>();
    EXPECT_CALL(*composition_trace, NumColumns()).WillRepeatedly(Return(n_columns));
    composition_trace_ptr = std::move(composition_trace);
  }

  // Create masks and trace decommitment callbacks.
  // uninitialized_element is used as a workaround to infer the underlying FieldElement type in the
  // commited trace.
  std::vector<std::pair<int64_t, uint64_t>> mask;
  auto create_mask = [&](size_t trace_i, const auto& trace, const auto& uninitialized_element) {
    using FieldElementT = std::decay_t<decltype(uninitialized_element)>;

    const size_t masks_items_in_trace = prng.UniformInt<size_t>(1, 5);
    const size_t column_offset = n_columns * trace_i;
    for (size_t i = 0; i < masks_items_in_trace; ++i) {
      mask.emplace_back(i, prng.UniformInt<size_t>(0, n_columns - 1) + column_offset);
    }

    EXPECT_CALL(
        *trace, VerifyDecommitment(Property(
                    &gsl::span<const std::tuple<uint64_t, uint64_t, size_t>>::size,
                    n_queries * masks_items_in_trace)))
        .WillOnce(Invoke([](gsl::span<const std::tuple<uint64_t, uint64_t, size_t>> queries) {
          return FieldElementT::UninitializedVector(queries.size());
        }));
  };
  create_mask(0, trace_ptr, BaseFieldElement::Uninitialized());
  const size_t first_mask_size = mask.size();
  if (with_composition_trace) {
    create_mask(1, composition_trace_ptr, ExtensionFieldElement::Uninitialized());
  }

  // Generate random queries.
  std::vector<std::pair<uint64_t, uint64_t>> queries;
  queries.reserve(n_queries);
  for (size_t i = 0; i < n_queries - 1; ++i) {
    queries.emplace_back(
        prng.UniformInt<uint64_t>(0, n_cosets - 1), prng.UniformInt<uint64_t>(0, trace_length - 1));
  }
  // Force a duplicate.
  queries.push_back(queries.back());

  // CompositionPolynomial::EvalAtPoint() callback.
  auto random_element = ExtensionFieldElement::RandomElement(&prng);
  EXPECT_CALL(
      composition_polynomial,
      EvalAtPoint(
          testing::Matcher<const BaseFieldElement&>(_), BeginEndDistanceIs(first_mask_size),
          BeginEndDistanceIs(mask.size() - first_mask_size)))
      .Times(n_queries)
      .WillRepeatedly(Return(random_element));

  // Create CompositionOracleVerifier.
  VerifierChannelMock channel;
  CompositionOracleVerifier oracle_verifier(
      UseOwned(&evaluation_domain), TakeOwnershipFrom(std::move(trace_ptr)),
      TakeOwnershipFrom(std::move(composition_trace_ptr)), mask, nullptr,
      UseOwned(&composition_polynomial), &channel);
  oracle_verifier.VerifyDecommitment(queries);
}

TEST(CompositionOracleVerifier, VerifyDecommitmentNoComposition) {
  VerifierTestVerifyDecommitment(16, 4, 7, false, 5);
}

TEST(CompositionOracleVerifier, VerifyDecommitmentWithComposition) {
  VerifierTestVerifyDecommitment(16, 4, 7, true, 5);
}

}  // namespace
}  // namespace starkware
