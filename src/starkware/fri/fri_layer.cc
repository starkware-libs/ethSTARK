#include "starkware/fri/fri_layer.h"

#include <algorithm>

#include "starkware/algebra/lde/lde_manager.h"

namespace starkware {

using starkware::fri::details::FriFolder;

//  Class FriLayer.

/*
  Gets the evaluation of the current layer as a vector.
*/
std::vector<ExtensionFieldElement> FriLayer::GetLayer() const {
  auto layer = ExtensionFieldElement::UninitializedVector(LayerSize());
  const auto layer_span = gsl::make_span(layer);
  GetLayerImpl(layer_span);
  return layer;
}

//  Class FriLayerReal.

void FriLayerReal::GetLayerImpl(gsl::span<ExtensionFieldElement> output) const {
  ASSERT_RELEASE(
      output.size() == evaluation_.size(), "output size is different than evaluation_ size.");
  std::copy(evaluation_.begin(), evaluation_.end(), output.begin());
}

std::vector<ExtensionFieldElement> FriLayerReal::EvalAtPoints(
    gsl::span<const uint64_t> required_indices) const {
  std::vector<ExtensionFieldElement> res;
  res.reserve(required_indices.size());
  for (uint64_t i : required_indices) {
    res.push_back(evaluation_[i]);
  }
  return res;
}

//  Class FriLayerProxy.

FriLayerProxy::FriLayerProxy(
    MaybeOwnedPtr<const FriLayer> prev_layer, const ExtensionFieldElement& eval_point)
    : FriLayer(FoldDomain(prev_layer->GetDomain())),
      prev_layer_(std::move(prev_layer)),
      eval_point_(eval_point) {}

void FriLayerProxy::GetLayerImpl(gsl::span<ExtensionFieldElement> output) const {
  const auto prev_layer_domain = prev_layer_->GetDomain();
  auto prev_eval = prev_layer_->GetLayer();
  FriFolder::ComputeNextFriLayer(prev_layer_domain, prev_eval, eval_point_, output);
}

std::vector<ExtensionFieldElement> FriLayerProxy::EvalAtPoints(
    gsl::span<const uint64_t> /* required_indices */) const {
  THROW_STARKWARE_EXCEPTION("Should never be called");
}

}  // namespace starkware
