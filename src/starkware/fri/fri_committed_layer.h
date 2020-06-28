#ifndef STARKWARE_FRI_FRI_COMMITTED_LAYER_H_
#define STARKWARE_FRI_FRI_COMMITTED_LAYER_H_

#include <memory>
#include <utility>
#include <vector>

#include "starkware/channel/prover_channel.h"
#include "starkware/commitment_scheme/table_prover.h"
#include "starkware/fri/fri_layer.h"
#include "starkware/fri/fri_parameters.h"

namespace starkware {

/*
  Represents a commitment over a FriLayer. Allows decommiting any index in the layer.
*/
class FriCommittedLayer {
 public:
  explicit FriCommittedLayer(size_t fri_step) : fri_step_(fri_step) {}
  virtual ~FriCommittedLayer() = default;
  virtual void Decommit(const std::vector<uint64_t>& queries) = 0;

 protected:
  const size_t fri_step_;
};

using FirstLayerCallback = std::function<void(const std::vector<uint64_t>& queries)>;

/*
  Wrapper over a callback function used for decommitment.
*/
class FriCommittedLayerByCallback : public FriCommittedLayer {
 public:
  FriCommittedLayerByCallback(
      size_t fri_step, MaybeOwnedPtr<FirstLayerCallback> layer_queries_callback)
      : FriCommittedLayer(fri_step), layer_queries_callback_(std::move(layer_queries_callback)) {}

  void Decommit(const std::vector<uint64_t>& queries) override;

 private:
  MaybeOwnedPtr<FirstLayerCallback> layer_queries_callback_;
};

/*
  Commits on a FriLayer using a TableProver.
*/
class FriCommittedLayerByTableProver : public FriCommittedLayer {
 public:
  FriCommittedLayerByTableProver(
      size_t fri_step, MaybeOwnedPtr<const FriLayer> layer,
      const TableProverFactory<ExtensionFieldElement>& table_prover_factory,
      const FriParameters& params, size_t layer_num);

  void Decommit(const std::vector<uint64_t>& queries) override;

 private:
  void Commit();
  struct ElementsData {
    std::vector<gsl::span<const ExtensionFieldElement>> elements;
    std::vector<std::vector<ExtensionFieldElement>> raw_data;
  };

  ElementsData EvalAtPoints(gsl::span<const uint64_t> required_row_indices);

  MaybeOwnedPtr<const FriLayer> fri_layer_;
  const FriParameters& params_;
  const size_t layer_num_;
  std::unique_ptr<TableProver<ExtensionFieldElement>> table_prover_;
};

}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_COMMITTED_LAYER_H_
