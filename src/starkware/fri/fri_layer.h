#ifndef STARKWARE_FRI_FRI_LAYER_H_
#define STARKWARE_FRI_FRI_LAYER_H_

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "starkware/fri/fri_folder.h"
#include "starkware/fri/fri_parameters.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

/*
  Base class of all FRI layers.
  It contains the implementation of mutual behavior and data of all types of layers and interfaces
  for specific required applications.
*/
class FriLayer {
 public:
  explicit FriLayer(const Coset& domain) : domain_(domain), layer_size_(domain_.Size()) {}

  virtual ~FriLayer() = default;
  FriLayer(FriLayer&&) = default;

  FriLayer(const FriLayer&) = delete;
  const FriLayer& operator=(const FriLayer&) = delete;
  const FriLayer& operator=(FriLayer&&) = delete;

  // The size of the whole layer.
  uint64_t LayerSize() const { return layer_size_; }

  // Get evaluation at specific indices.
  virtual std::vector<ExtensionFieldElement> EvalAtPoints(
      gsl::span<const uint64_t> required_indices) const = 0;

  const Coset& GetDomain() const { return domain_; }

  // Get the evaluation of current layer as a vector.
  virtual std::vector<ExtensionFieldElement> GetLayer() const;

 protected:
  virtual void GetLayerImpl(gsl::span<ExtensionFieldElement> output) const = 0;

 private:
  Coset domain_;

 protected:
  const uint64_t layer_size_;  // Caching the layer size for loop optimization.
};

/*
  Represents a real FRI layer (as opposed to a proxy layer). There must be at least one proxy layer
  between every two real layers.
*/
class FriLayerReal : public FriLayer {
 public:
  explicit FriLayerReal(MaybeOwnedPtr<const FriLayer> prev_layer)
      : FriLayer(prev_layer->GetDomain()), evaluation_(prev_layer->GetLayer()) {}
  explicit FriLayerReal(std::vector<ExtensionFieldElement>&& evaluation, const Coset& domain)
      : FriLayer(domain), evaluation_(std::move(evaluation)) {}

  std::vector<ExtensionFieldElement> EvalAtPoints(
      gsl::span<const uint64_t> required_indices) const override;

 protected:
  void GetLayerImpl(gsl::span<ExtensionFieldElement> output) const override;

 private:
  std::vector<ExtensionFieldElement> evaluation_;
};

/*
  A layer which is used as a proxy between layers. The proxy is the only kind of layer which folds
  the domain, so there must be at least one proxy between every two other types of layers.
*/
class FriLayerProxy : public FriLayer {
 public:
  FriLayerProxy(MaybeOwnedPtr<const FriLayer> prev_layer, const ExtensionFieldElement& eval_point);

  std::vector<ExtensionFieldElement> EvalAtPoints(
      gsl::span<const uint64_t> required_indices) const override;

 protected:
  void GetLayerImpl(gsl::span<ExtensionFieldElement> output) const override;

 private:
  Coset FoldDomain(const Coset& domain) { return GetCosetForFriLayer(domain, 1); }

  MaybeOwnedPtr<const FriLayer> prev_layer_;
  const ExtensionFieldElement eval_point_;
};

}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_LAYER_H_
