#include "starkware/fri/fri_committed_layer.h"
#include "starkware/fri/fri_details.h"

#include "gtest/gtest.h"
#include "starkware/algebra/lde/lde_manager.h"
#include "starkware/channel/prover_channel_mock.h"
#include "starkware/fri/fri_folder.h"
#include "starkware/fri/fri_parameters.h"
#include "starkware/fri/fri_test_utils.h"
#include "starkware/stark/utils.h"

namespace starkware {
namespace fri {
namespace layer {
namespace {

using testing::_;
using testing::AtLeast;
using testing::MatcherCast;

TEST(FriCommittedLayerTest, FriCommittedLayerCallback) {
  auto callback = FirstLayerCallback([](const std::vector<uint64_t>& queries) {
    EXPECT_EQ(queries, std::vector<uint64_t>({4, 5, 8, 9, 12, 13}));
    return queries;
  });
  auto layer = FriCommittedLayerByCallback(1, UseOwned(&callback));
  const std::vector<uint64_t> queries{2, 4, 6};
  layer.Decommit(queries);
}

TEST(FriCommittedLayerTest, FriCommittedLayerByTableProver) {
  Prng prng;
  const size_t log2_eval_domain = 10;
  const size_t last_layer_degree_bound = 5;
  const size_t proof_of_work_bits = 15;
  const Coset eval_domain(Pow2(log2_eval_domain), BaseFieldElement::One());

  FriParameters params({
      /*fri_step_list=*/{2, 3, 1},
      /*last_layer_degree_bound=*/last_layer_degree_bound,
      /*n_queries=*/2,
      /*domain=*/eval_domain,
      /*proof_of_work_bits=*/proof_of_work_bits,
  });

  ProverChannelMock prover_channel;
  EXPECT_CALL(prover_channel, SendBytes(_)).Times(AtLeast(9));
  EXPECT_CALL(prover_channel, SendFieldElementImpl(MatcherCast<const ExtensionFieldElement&>(_)))
      .Times(1);

  TableProverFactory<ExtensionFieldElement> table_prover_factory =
      [&](uint64_t n_segments, uint64_t n_rows_per_segment, size_t n_columns) {
        EXPECT_EQ(n_segments, 1);
        EXPECT_EQ(n_rows_per_segment, 256);
        EXPECT_EQ(n_columns, 2);
        return GetTableProverFactory<ExtensionFieldElement>(&prover_channel)(
            n_segments, n_rows_per_segment, n_columns);
      };

  auto eval_point = ExtensionFieldElement::RandomElement(&prng);

  // Construct a witness with the size of the entire evaluation domain.
  details::TestPolynomial test_layer(&prng, 64 * last_layer_degree_bound);
  std::vector<ExtensionFieldElement> eval_domain_data = test_layer.GetData(eval_domain);

  FriLayerReal layer_0_in(std::move(eval_domain_data), eval_domain);
  FriLayerProxy layer_1_proxy(UseOwned(&layer_0_in), eval_point);
  FriLayerReal layer_2_in(UseOwned(&layer_1_proxy));

  FriCommittedLayerByTableProver committed_layer(
      1, UseOwned(&layer_2_in), table_prover_factory, params, 2);

  const std::vector<uint64_t> queries{2, 4, 6};
  committed_layer.Decommit(queries);
}

}  // namespace
}  // namespace layer
}  // namespace fri
}  // namespace starkware
