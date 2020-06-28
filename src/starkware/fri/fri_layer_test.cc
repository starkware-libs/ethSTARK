#include "starkware/fri/fri_layer.h"

#include "gtest/gtest.h"
#include "starkware/algebra/lde/lde_manager.h"
#include "starkware/fri/fri_folder.h"
#include "starkware/fri/fri_parameters.h"
#include "starkware/fri/fri_test_utils.h"

namespace starkware {
namespace fri {
namespace layer {
namespace {

using starkware::fri::details::FriFolder;

/*
  Contains data and functions to avoid boiler plate and duplicate code.
*/
class FriLayerTest : public ::testing::Test {
 public:
  FriLayerTest() : domain_(Pow2(log2_eval_domain_), BaseFieldElement::One()) {}

 protected:
  std::vector<ExtensionFieldElement> CreateEvaluation(Prng* prng, size_t prefix_size = 0) {
    details::TestPolynomial test_layer(prng, first_layer_degree_bound_);

    if (prefix_size > 0) {
      std::vector<ExtensionFieldElement> domain_eval = test_layer.GetData(domain_);
      witness_data_.assign(domain_eval.begin(), domain_eval.begin() + prefix_size);
    } else {
      witness_data_ = test_layer.GetData(domain_);
    }
    return witness_data_;
  }

  void Init() {
    Prng prng;

    std::vector<ExtensionFieldElement> evaluation =
        CreateEvaluation(&prng, Pow2(log2_eval_domain_));

    eval_point_ = std::make_optional(ExtensionFieldElement::RandomElement(&prng));

    layer_0_ = std::make_unique<FriLayerReal>(std::move(evaluation), domain_);
    layer_1_proxy_ = std::make_unique<FriLayerProxy>(UseOwned(layer_0_), *eval_point_);
  }

  void InitMoreLayers() {
    layer_2_ = std::make_unique<FriLayerReal>(UseOwned(layer_1_proxy_));
    layer_3_proxy_ = std::make_unique<FriLayerProxy>(UseOwned(layer_2_), *eval_point_);
    layer_4_ = std::make_unique<FriLayerReal>(UseOwned(layer_3_proxy_));
  }

  std::unique_ptr<FriLayer> layer_0_;
  std::unique_ptr<FriLayer> layer_1_proxy_;
  std::unique_ptr<FriLayer> layer_2_;
  std::unique_ptr<FriLayer> layer_3_proxy_;
  std::unique_ptr<FriLayer> layer_4_;

  std::optional<ExtensionFieldElement> eval_point_;
  std::vector<ExtensionFieldElement> witness_data_;

  const size_t log2_eval_domain_ = 10;
  const size_t first_layer_degree_bound_ = 320;
  const Coset domain_;
};

TEST_F(FriLayerTest, FriLayerSize) {
  this->Init();
  this->InitMoreLayers();

  EXPECT_EQ(this->layer_0_->LayerSize(), 1024);
  EXPECT_EQ(this->layer_1_proxy_->LayerSize(), 512);
  EXPECT_EQ(this->layer_2_->LayerSize(), 512);
  EXPECT_EQ(this->layer_3_proxy_->LayerSize(), 256);
  EXPECT_EQ(this->layer_4_->LayerSize(), 256);
}

TEST_F(FriLayerTest, FirstLayerDegreeCheck) {
  this->Init();

  std::vector<ExtensionFieldElement> layer_eval = this->layer_0_->GetLayer();

  auto layer_0_lde_manager =
      MakeLdeManager<ExtensionFieldElement>(this->layer_0_->GetDomain(), false);
  layer_0_lde_manager->AddEvaluation(std::move(layer_eval));
  EXPECT_EQ(layer_0_lde_manager->GetEvaluationDegree(0), this->first_layer_degree_bound_ - 1);
}

TEST_F(FriLayerTest, FirstLayer) {
  Prng prng;
  auto evaluation = prng.RandomFieldElementVector<ExtensionFieldElement>(this->domain_.Size());

  FriLayerReal layer_real(std::vector<ExtensionFieldElement>(evaluation), this->domain_);

  std::vector<ExtensionFieldElement> layer_eval = layer_real.GetLayer();

  EXPECT_EQ(evaluation, layer_eval);
}

TEST_F(FriLayerTest, LayerZero) {
  this->Init();

  auto layer_size = this->layer_0_->LayerSize();

  EXPECT_EQ(layer_size, this->witness_data_.size());
  EXPECT_EQ(this->layer_0_->GetLayer(), this->witness_data_);
}

TEST_F(FriLayerTest, ProxyLayer) {
  this->Init();

  std::vector<ExtensionFieldElement> prev_layer_eval = this->layer_0_->GetLayer();

  std::vector<ExtensionFieldElement> layer_eval = this->layer_1_proxy_->GetLayer();
  auto layer_1_lde_manager =
      MakeLdeManager<ExtensionFieldElement>(this->layer_1_proxy_->GetDomain(), false);
  layer_1_lde_manager->AddEvaluation(layer_eval);
  EXPECT_EQ(layer_1_lde_manager->GetEvaluationDegree(0), this->first_layer_degree_bound_ / 2 - 1);

  const std::vector<ExtensionFieldElement> folded_layer =
      FriFolder::ComputeNextFriLayer(this->domain_, prev_layer_eval, *this->eval_point_);

  EXPECT_EQ(layer_eval, folded_layer);
}

TEST_F(FriLayerTest, RealLayer) {
  this->Init();
  FriLayerReal layer_2(UseOwned(this->layer_1_proxy_));

  std::vector<ExtensionFieldElement> prev_layer_eval = this->layer_0_->GetLayer();

  std::vector<ExtensionFieldElement> layer_eval = layer_2.GetLayer();
  auto layer_2_lde_manager = MakeLdeManager<ExtensionFieldElement>(layer_2.GetDomain(), false);
  layer_2_lde_manager->AddEvaluation(layer_eval);
  EXPECT_EQ(layer_2_lde_manager->GetEvaluationDegree(0), this->first_layer_degree_bound_ / 2 - 1);

  std::vector<ExtensionFieldElement> folded_layer =
      FriFolder::ComputeNextFriLayer(this->domain_, prev_layer_eval, *this->eval_point_);

  EXPECT_EQ(layer_eval.size(), folded_layer.size());
  for (size_t i = 0; i < layer_eval.size(); ++i) {
    EXPECT_EQ(layer_eval[i], folded_layer[i]);
  }

  auto layer_size = layer_2.LayerSize();

  auto layer = layer_2.GetLayer();
  EXPECT_EQ(layer_size, layer.size());
  for (size_t i = 0; i < layer_size; ++i) {
    EXPECT_EQ(layer[i], folded_layer[i]);
  }
}

TEST_F(FriLayerTest, EvaluationTest) {
  this->Init();
  this->InitMoreLayers();

  uint64_t index = 42;
  std::vector<uint64_t> required_row_indices{index};
  std::vector<ExtensionFieldElement> layer_eval = this->layer_4_->GetLayer();
  auto evaluation = this->layer_4_->EvalAtPoints(required_row_indices);
  EXPECT_EQ(evaluation[0], layer_eval[index]);
}

}  // namespace
}  // namespace layer
}  // namespace fri
}  // namespace starkware
