#include "starkware/fri/fri_prover.h"

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/lde/lde_manager.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/channel/prover_channel.h"
#include "starkware/channel/prover_channel_mock.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/channel/verifier_channel_mock.h"
#include "starkware/commitment_scheme/commitment_scheme_builder.h"
#include "starkware/commitment_scheme/merkle/merkle_commitment_scheme.h"
#include "starkware/commitment_scheme/packaging_commitment_scheme.h"
#include "starkware/commitment_scheme/table_prover_impl.h"
#include "starkware/commitment_scheme/table_prover_mock.h"
#include "starkware/commitment_scheme/table_verifier_impl.h"
#include "starkware/commitment_scheme/table_verifier_mock.h"
#include "starkware/crypt_tools/blake2s_160.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/fri/fri_parameters.h"
#include "starkware/fri/fri_test_utils.h"
#include "starkware/fri/fri_verifier.h"
#include "starkware/proof_system/proof_system.h"

namespace starkware {
namespace {

using starkware::fri::details::FriFolder;
using starkware::fri::details::TestPolynomial;
using testing::_;
using testing::ElementsAre;
using testing::HasSubstr;
using testing::Invoke;
using testing::MockFunction;
using testing::Return;
using testing::StrictMock;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

TEST(FriTest, ProverBasicFlowWithMockChannel) {
  Prng prng;

  const size_t log2_eval_domain = 10;
  const size_t last_layer_degree_bound = 5;
  const size_t proof_of_work_bits = 15;
  const Coset domain(Pow2(log2_eval_domain), BaseFieldElement::One());
  FriParameters params{
      /*fri_step_list=*/{2, 3, 1},
      /*last_layer_degree_bound=*/last_layer_degree_bound,
      /*n_queries=*/2,
      /*domain=*/domain,
      /*proof_of_work_bits=*/proof_of_work_bits,
  };

  TestPolynomial test_layer(&prng, 64 * last_layer_degree_bound);

  // Construct a witness with the size of the entire evaluation domain.
  std::vector<ExtensionFieldElement> eval_domain_data = test_layer.GetData(domain);

  const ExtensionFieldElement eval_point = ExtensionFieldElement::RandomElement(&prng);

  const std::vector<ExtensionFieldElement> second_layer = FriFolder::ComputeNextFriLayer(
      params.GetCosetForLayer(1),
      FriFolder::ComputeNextFriLayer(domain, eval_domain_data, eval_point), Pow(eval_point, 2));
  const std::vector<ExtensionFieldElement> third_layer = FriFolder::ComputeNextFriLayer(
      params.GetCosetForLayer(4),
      FriFolder::ComputeNextFriLayer(
          params.GetCosetForLayer(3),
          FriFolder::ComputeNextFriLayer(
              params.GetCosetForLayer(2), second_layer, Pow(eval_point, 4)),
          Pow(eval_point, 8)),
      Pow(eval_point, 16));

  // last_layer_coefs will contain the coefficients of the last layer, as sent on the channel.
  std::vector<ExtensionFieldElement> last_layer_coefs;

  // Set mock expectations.
  StrictMock<ProverChannelMock> prover_channel;
  TableProverMockFactory<ExtensionFieldElement> table_prover_factory(
      {std::make_tuple(1, 32, 8), std::make_tuple(1, 16, 2)});
  StrictMock<MockFunction<void(const std::vector<uint64_t>& queries)>> first_layer_queries_callback;
  {
    testing::InSequence dummy;

    // Commit phase expectations.

    // The prover will request three evaluation points. Answer with eval_point, eval_point^4 and
    // eval_point^32. This sequence will allow testing the polynomial of the last layer. See below.
    EXPECT_CALL(prover_channel, ReceiveFieldElementImpl()).WillOnce(Return(eval_point));
    EXPECT_CALL(table_prover_factory[0], AddSegmentForCommitment(_, 0, 8));
    EXPECT_CALL(table_prover_factory[0], Commit());
    EXPECT_CALL(prover_channel, ReceiveFieldElementImpl()).WillOnce(Return(Pow(eval_point, 4)));
    EXPECT_CALL(table_prover_factory[1], AddSegmentForCommitment(_, 0, 2));
    EXPECT_CALL(table_prover_factory[1], Commit());
    EXPECT_CALL(prover_channel, ReceiveFieldElementImpl()).WillOnce(Return(Pow(eval_point, 32)));

    // The prover will send the coefficients of the last layer. Save those in last_layer_coefs.
    EXPECT_CALL(prover_channel, SendFieldElementSpanImpl(_))
        .WillOnce(Invoke([&last_layer_coefs](gsl::span<const ExtensionFieldElement> values) {
          last_layer_coefs = {values.begin(), values.end()};
        }));

    // Query phase expectations.
    // Proof of work.
    EXPECT_CALL(prover_channel, ApplyProofOfWork(proof_of_work_bits));

    // The prover will request two query locations. Answer with 0 and 6.
    EXPECT_CALL(prover_channel, ReceiveNumberImpl(256)).WillOnce(Return(0)).WillOnce(Return(6));
    // The verifier requested indices 0 and 6 which refer to the two cosets (0, 1, 2, 3) and (24,
    // 25, 26, 27) in the first layer (x -> (4 * x, ..., 4 * x + 3)). Hence, the prover will send
    // data[0], ..., data[3], data[24], ..., data[27] from the top layer.
    // Handling the first layer is done using a callback to first_layer_queries_callback.
    EXPECT_CALL(first_layer_queries_callback, Call(ElementsAre(0, 1, 2, 3, 24, 25, 26, 27)));

    // Decommitment phase expectations.

    // As the verifier requested indices 0 and 6 (which refer to (0, 1, 2, 3) and (24, 25, 26,
    // 27) in the first layer), it will be able to compute the values at indices 0 and 6 of the
    // second layer of FRI. The prover will additionally send the values at indices 1...5, 7
    // which will allow the verifier to compute index 0 on the third layer. Then it will send
    // index 1 of the third layer to allow the verifier to continue to the forth (and last)
    // layer.
    // We mock StartDecommitmentPhase() to ask for rows 0,...,9.
    const std::vector<uint64_t> simulated_requested_rows = {0};
    EXPECT_CALL(
        table_prover_factory[0],
        StartDecommitmentPhase(
            UnorderedElementsAreArray({RowCol(0, 1), RowCol(0, 2), RowCol(0, 3), RowCol(0, 4),
                                       RowCol(0, 5), RowCol(0, 7)}),
            UnorderedElementsAre(RowCol(0, 0), RowCol(0, 6))))
        .WillOnce(Return(simulated_requested_rows));
    EXPECT_CALL(table_prover_factory[0], Decommit(_))
        .WillOnce(
            Invoke([&](gsl::span<const gsl::span<const ExtensionFieldElement>> elements_data) {
              EXPECT_EQ(elements_data.size(), 8);
              for (size_t i = 0; i < 8; ++i) {
                EXPECT_EQ(elements_data[i][0], second_layer[i]);
              }
            }));

    EXPECT_CALL(
        table_prover_factory[1],
        StartDecommitmentPhase(
            UnorderedElementsAre(RowCol(0, 1)), UnorderedElementsAre(RowCol(0, 0))))
        .WillOnce(Return(std::vector<uint64_t>{}));

    EXPECT_CALL(table_prover_factory[1], Decommit(_))
        .WillOnce(
            Invoke([&](gsl::span<const gsl::span<const ExtensionFieldElement>> elements_data) {
              const std::vector<ExtensionFieldElement> empty_vector;
              EXPECT_THAT(elements_data, ElementsAre(empty_vector, empty_vector));
            }));
  }
  TableProverFactory<ExtensionFieldElement> table_prover_factory_as_factory =
      table_prover_factory.AsFactory();
  FriProver::FirstLayerCallback first_layer_queries_callback_as_function =
      first_layer_queries_callback.AsStdFunction();
  FriProver fri_prover(
      UseOwned(&prover_channel), UseOwned(&table_prover_factory_as_factory), UseOwned(&params),
      std::move(eval_domain_data), UseOwned(&first_layer_queries_callback_as_function));
  fri_prover.ProveFri();

  // In FRI, if the verifier sends a sequence of evaluation points of the form
  //   x_0, x_0^{2^fri_step_list[0]}, x_0^{2^{fri_step_list[0] + fri_step_list[1]}}, ...,
  // the polynomial p(y) of the last layer will satisfy
  //   p(x_0^{2^{fri_step_list[0] + ...}}) = f(x_0).
  // Since we skip the division by 2 in FRI, the expected result is 2^n f(x_0) where n is the sum
  // of fri_step_list.
  EXPECT_EQ(last_layer_degree_bound, last_layer_coefs.size());

  const ExtensionFieldElement correction_factor = ExtensionFieldElement::FromUint(64);

  // Evaluate the last layer polynomial at a point.
  const ExtensionFieldElement test_value = ExtrapolatePointFromCoefficients(
      params.GetCosetForLayer(6), last_layer_coefs, Pow(eval_point, 64));

  EXPECT_EQ(correction_factor * test_layer.EvalAt(eval_point), test_value);
}

TEST(FriTest, VerifierBasicFlowWithMockChannel) {
  Prng prng;
  const size_t last_layer_degree_bound = 5;
  const size_t proof_of_work_bits = 15;
  const size_t log2_eval_domain = 10;

  const auto offset = BaseFieldElement::RandomElement(&prng);
  const Coset domain(Pow2(log2_eval_domain), offset);
  FriParameters params{
      /*fri_step_list=*/{2, 3, 1},
      /*last_layer_degree_bound=*/last_layer_degree_bound,
      /*n_queries=*/2,
      /*domain=*/domain,
      /*proof_of_work_bits=*/proof_of_work_bits,
  };

  TestPolynomial test_layer(&prng, 64 * last_layer_degree_bound);
  std::vector<ExtensionFieldElement> witness_data = test_layer.GetData(domain);
  // Choose evaluation points for the three layers.
  const std::vector<ExtensionFieldElement> eval_points =
      prng.RandomFieldElementVector<ExtensionFieldElement>(3);

  // Set mock expectations.
  StrictMock<VerifierChannelMock> verifier_channel;
  TableVerifierMockFactory<ExtensionFieldElement> table_verifier_factory(
      {std::make_pair(32, 8), std::make_pair(16, 2)});
  StrictMock<MockFunction<std::vector<ExtensionFieldElement>(const std::vector<uint64_t>& queries)>>
      first_layer_queries_callback;

  const std::vector<ExtensionFieldElement> second_layer = FriFolder::ComputeNextFriLayer(
      params.GetCosetForLayer(1),
      FriFolder::ComputeNextFriLayer(domain, witness_data, eval_points[0]), Pow(eval_points[0], 2));
  const std::vector<ExtensionFieldElement> third_layer = FriFolder::ComputeNextFriLayer(
      params.GetCosetForLayer(4),
      FriFolder::ComputeNextFriLayer(
          params.GetCosetForLayer(3),
          FriFolder::ComputeNextFriLayer(params.GetCosetForLayer(2), second_layer, eval_points[1]),
          Pow(eval_points[1], 2)),
      Pow(eval_points[1], 4));

  const std::vector<ExtensionFieldElement> fourth_layer =
      FriFolder::ComputeNextFriLayer(params.GetCosetForLayer(5), third_layer, eval_points[2]);

  std::unique_ptr<LdeManager<ExtensionFieldElement>> fourth_layer_lde =
      MakeLdeManager<ExtensionFieldElement>(
          params.GetCosetForLayer(6), /*eval_in_natural_order=*/false);
  fourth_layer_lde->AddEvaluation(fourth_layer);
  EXPECT_EQ(fourth_layer_lde->GetEvaluationDegree(0), last_layer_degree_bound - 1);
  const gsl::span<const ExtensionFieldElement> fourth_layer_coefs =
      fourth_layer_lde->GetCoefficients(0);

  {
    testing::InSequence dummy;

    // Commit phase expectations.
    // The verifier will send three elements - eval_points[0], eval_points[1] and eval_points[2].
    EXPECT_CALL(verifier_channel, GetAndSendRandomFieldElementImpl())
        .WillOnce(Return(eval_points[0]));
    EXPECT_CALL(table_verifier_factory[0], ReadCommitment());
    EXPECT_CALL(verifier_channel, GetAndSendRandomFieldElementImpl())
        .WillOnce(Return(eval_points[1]));
    EXPECT_CALL(table_verifier_factory[1], ReadCommitment());
    EXPECT_CALL(verifier_channel, GetAndSendRandomFieldElementImpl())
        .WillOnce(Return(eval_points[2]));
    // The verifier will read the last layer coefficents.
    EXPECT_CALL(verifier_channel, ReceiveFieldElementSpanImpl(_))
        .WillOnce(Invoke([fourth_layer_coefs](gsl::span<ExtensionFieldElement> span) {
          ASSERT_RELEASE(
              span.size() == last_layer_degree_bound,
              "span size is not equal to last layer degree bound.");
          for (size_t i = 0; i < last_layer_degree_bound; ++i) {
            span.at(i) = fourth_layer_coefs[i];
          }
        }));

    // Query phase expectations.
    // Proof of work.
    EXPECT_CALL(verifier_channel, ApplyProofOfWork(proof_of_work_bits));

    // The verifier will send two query locations - 0 and 6.
    EXPECT_CALL(verifier_channel, GetAndSendRandomNumberImpl(256))
        .WillOnce(Return(0))
        .WillOnce(Return(6));

    // First Layer.
    // The received cosets for queries 0 and 6, are (0, 1, 2, 3) and (24, 25, 26, 27) respectively.
    // Upon calling the first_layer_queries_callback, the witness at the these 8 indices will be
    // given.
    std::vector<ExtensionFieldElement> witness_elements = {
        {(witness_data[0]), (witness_data[1]), (witness_data[2]), (witness_data[3]),
         (witness_data[24]), (witness_data[25]), (witness_data[26]), (witness_data[27])}};

    EXPECT_CALL(first_layer_queries_callback, Call(ElementsAre(0, 1, 2, 3, 24, 25, 26, 27)))
        .WillOnce(Return(witness_elements));

    // Second Layer.
    // Fake response from prover on the data queries.
    std::set<RowCol> data_query_indices{RowCol(0, 1), RowCol(0, 2), RowCol(0, 3),
                                        RowCol(0, 4), RowCol(0, 5), RowCol(0, 7)};
    std::set<RowCol> integrity_query_indices{RowCol(0, 0), RowCol(0, 6)};
    std::map<RowCol, ExtensionFieldElement> data_queries_response;

    for (const RowCol& query : data_query_indices) {
      data_queries_response.insert({query, second_layer.at(query.GetCol())});
    }

    EXPECT_CALL(
        table_verifier_factory[0], Query(
                                       UnorderedElementsAreArray(data_query_indices),
                                       UnorderedElementsAreArray(integrity_query_indices)))
        .WillOnce(Return(data_queries_response));

    // Add integrity queries to the map, and send this data to verification.
    data_queries_response.insert({RowCol(0, 0), second_layer.at(0)});
    data_queries_response.insert({RowCol(0, 6), second_layer.at(6)});

    EXPECT_CALL(
        table_verifier_factory[0],
        VerifyDecommitment(UnorderedElementsAreArray(data_queries_response)))
        .WillOnce(Return(true));

    // Third Layer.
    data_query_indices = {RowCol(0, 1)};
    integrity_query_indices = {RowCol(0, 0)};
    data_queries_response = {{RowCol(0, 1), third_layer.at(1)}};
    EXPECT_CALL(
        table_verifier_factory[1], Query(
                                       UnorderedElementsAreArray(data_query_indices),
                                       UnorderedElementsAreArray(integrity_query_indices)))
        .WillOnce(Return(data_queries_response));

    // Add integrity queries to the map, and send this data to verification.
    data_queries_response.insert({RowCol(0, 0), third_layer.at(0)});
    EXPECT_CALL(
        table_verifier_factory[1],
        VerifyDecommitment(UnorderedElementsAreArray(data_queries_response)))
        .WillOnce(Return(true));
  }
  TableVerifierFactory<ExtensionFieldElement> table_verifier_factory_as_factory =
      table_verifier_factory.AsFactory();
  FriVerifier::FirstLayerCallback first_layer_queries_callback_as_function =
      first_layer_queries_callback.AsStdFunction();
  FriVerifier fri_verifier(
      UseOwned(&verifier_channel), UseOwned(&table_verifier_factory_as_factory), UseOwned(&params),
      UseOwned(&first_layer_queries_callback_as_function));
  fri_verifier.VerifyFri();
  LOG(INFO) << verifier_channel;
}

struct FriTestDefinitions0 {
  static constexpr std::array<size_t, 4> kFriStepList = {0, 2, 1, 4};
};

struct FriTestDefinitions1 {
  static constexpr std::array<size_t, 3> kFriStepList = {2, 1, 4};
};

struct FriTestDefinitions2 {
  static constexpr std::array<size_t, 3> kFriStepList = {0, 4, 3};
};

using EndToEndDefinitions =
    ::testing::Types<FriTestDefinitions0, FriTestDefinitions1, FriTestDefinitions2>;

template <typename TestDefinitionsT>
class FriEndToEndTest : public testing::Test {
 public:
  FriEndToEndTest()
      : params{fri_step_list, last_layer_degree_bound, n_queries,
               Coset(eval_domain_size, BaseFieldElement::RandomElement(&prng)),
               proof_of_work_bits} {}

  void InitWitness(size_t degree_bound, size_t prefix_size) {
    TestPolynomial test_layer(&prng, degree_bound);

    eval_domain_data = test_layer.GetData(params.domain);
    ASSERT_LE(prefix_size, eval_domain_data.size());
    std::vector<ExtensionFieldElement> prefix_data(
        eval_domain_data.begin(), eval_domain_data.begin() + prefix_size);
    witness = std::move(prefix_data);
  }

  std::vector<std::byte> GenerateProof() { return GenerateProofWithAnnotations().first; }

  std::pair<std::vector<std::byte>, std::vector<std::string>> GenerateProofWithAnnotations() {
    ProverChannel p_channel(channel_prng.Clone());

    TableProverFactory<ExtensionFieldElement> table_prover_factory =
        [&](size_t n_segments, uint64_t n_rows_per_segment, size_t n_columns) {
          auto packaging_commitment_scheme = MakeCommitmentSchemeProver(
              ExtensionFieldElement::SizeInBytes() * n_columns, n_rows_per_segment, n_segments,
              &p_channel);

          return std::make_unique<TableProverImpl<ExtensionFieldElement>>(
              n_columns, UseMovedValue(std::move(packaging_commitment_scheme)), &p_channel);
        };

    FriProver::FirstLayerCallback first_layer_queries_callback =
        [](const std::vector<uint64_t>& /* queries */) {};

    // Create a FRI proof.
    FriProver fri_prover(
        UseOwned(&p_channel), UseOwned(&table_prover_factory), UseOwned(&params),
        std::move(*witness), UseOwned(&first_layer_queries_callback));
    fri_prover.ProveFri();
    return {p_channel.GetProof(), p_channel.GetAnnotations()};
  }

  bool VerifyProof(
      gsl::span<const std::byte> proof,
      const std::optional<std::vector<std::string>>& prover_annotations = std::nullopt) {
    VerifierChannel v_channel(channel_prng.Clone(), proof);
    if (prover_annotations) {
      v_channel.SetExpectedAnnotations(*prover_annotations);
    }

    TableVerifierFactory<ExtensionFieldElement> table_verifier_factory = [&](uint64_t n_rows,
                                                                             size_t n_columns) {
      auto packaging_commitment_scheme = MakeCommitmentSchemeVerifier(
          ExtensionFieldElement::SizeInBytes() * n_columns, n_rows, &v_channel);

      return std::make_unique<TableVerifierImpl<ExtensionFieldElement>>(
          n_columns, UseMovedValue(std::move(packaging_commitment_scheme)), &v_channel);
    };

    FriVerifier::FirstLayerCallback first_layer_queries_callback =
        [&](const std::vector<uint64_t>& queries) {
          std::vector<ExtensionFieldElement> res;
          res.reserve(queries.size());
          for (uint64_t query : queries) {
            res.emplace_back(eval_domain_data.at(query));
          }
          return res;
        };

    return FalseOnError([&]() {
      FriVerifier fri_verifier(
          UseOwned(&v_channel), UseOwned(&table_verifier_factory), UseOwned(&params),
          UseOwned(&first_layer_queries_callback));
      fri_verifier.VerifyFri();
    });
  }

  const size_t log2_eval_domain = 10;
  const size_t eval_domain_size = Pow2(log2_eval_domain);
  const size_t n_layers = 7;
  const std::vector<size_t> fri_step_list{TestDefinitionsT::kFriStepList.begin(),
                                          TestDefinitionsT::kFriStepList.end()};
  const uint64_t last_layer_degree_bound = 3;
  const uint64_t degree_bound = Pow2(n_layers) * last_layer_degree_bound;
  const size_t n_queries = 4;
  const size_t proof_of_work_bits = 15;
  Prng prng;
  const Prng channel_prng;
  FriParameters params;
  std::optional<std::vector<ExtensionFieldElement>> witness;
  std::vector<ExtensionFieldElement> eval_domain_data;
};

TYPED_TEST_CASE(FriEndToEndTest, EndToEndDefinitions);

TYPED_TEST(FriEndToEndTest, Correctness) {
  this->InitWitness(this->degree_bound, this->eval_domain_size);

  const auto proof_annotations_pair = this->GenerateProofWithAnnotations();
  // VerifyProof should return true with or without checking the annotations.
  EXPECT_TRUE(this->VerifyProof(proof_annotations_pair.first, proof_annotations_pair.second));
  EXPECT_TRUE(this->VerifyProof(proof_annotations_pair.first));
}

TYPED_TEST(FriEndToEndTest, NegativeTestLargerDegree) {
  this->InitWitness(this->degree_bound + 1, this->eval_domain_size);

  EXPECT_ASSERT(this->GenerateProof(), HasSubstr("Last FRI layer is of degree"));
}

TYPED_TEST(FriEndToEndTest, ChangeByte) {
  this->InitWitness(this->degree_bound, this->eval_domain_size);

  std::vector<std::byte> proof = this->GenerateProof();
  EXPECT_TRUE(this->VerifyProof(proof));

  const auto byte_idx = this->prng.template UniformInt<size_t>(0, proof.size() - 1);
  proof[byte_idx] ^= std::byte(this->prng.template UniformInt<uint8_t>(1, 255));

  EXPECT_FALSE(this->VerifyProof(proof));
}

}  // namespace
}  // namespace starkware
