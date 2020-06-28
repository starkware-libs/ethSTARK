#ifndef STARKWARE_FRI_FRI_VERIFIER_H_
#define STARKWARE_FRI_FRI_VERIFIER_H_

#include <memory>
#include <vector>

#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/fri/fri_details.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

class FriVerifier {
 public:
  using FirstLayerCallback =
      std::function<std::vector<ExtensionFieldElement>(const std::vector<uint64_t>& queries)>;

  FriVerifier(
      MaybeOwnedPtr<VerifierChannel> channel,
      MaybeOwnedPtr<const TableVerifierFactory<ExtensionFieldElement>> table_verifier_factory,
      MaybeOwnedPtr<const FriParameters> params,
      MaybeOwnedPtr<FirstLayerCallback> first_layer_queries_callback)
      : channel_(UseOwned(channel)),
        table_verifier_factory_(UseOwned(table_verifier_factory)),
        params_(UseOwned(params)),
        first_layer_queries_callback_(UseOwned(first_layer_queries_callback)),
        n_layers_(params_->fri_step_list.size()) {}

  /*
    Applies the FRI protocol to verify that the prover has access to a witness whose degree is
    smaller than 2^n_layers. first_layer_queries_callback is a callback function that will be called
    once, with the indices of the queries for the first layer. Given a vector of query indices, this
    callback is responsible for:
      1. Given the specific indices in the first layer - it returns their values to be used by
         VerifyFri().
      2. Verifying the correctness of the returned values against the prover's decommitments.
    In case the verification fails, it throws an exception.
  */
  void VerifyFri();

 private:
  /*
    Initializes local class variables.
  */
  void Init();

  /*
    For each layer - we send a random field element and obtain a commitment.
    The field elements and commitments are stored internally for the query phase.
  */
  void CommitmentPhase();

  /*
    Reads the coefficients of the interpolation polynomial of the last layer and computes
    expected_last_layer_.
  */
  void ReadLastLayerCoefficients();

  /*
    Calls the first_layer_queries_callback to send the queries through the appropriate "channel" and
    authenticate responses. Compute the next layer and stores it in query_results_.
  */
  void VerifyFirstLayer();

  /*
    For each of the inner layers (i.e. not the first nor the last), we send the queries through the
    appropriate "channel", and authenticate responses. We go layer-by-layer, resolving all queries
    in parallel. If all is correct - the last layer (not computed in this function) is expected to
    agree with expected_last_layer_. If a verification on some layer fails, it throws an exception.
  */
  void VerifyInnerLayers();

  /*
    Verifies that the elements of the last layer are consistent with expected_last_layer_.
    Throws an exception otherwise.
  */
  void VerifyLastLayer();

  MaybeOwnedPtr<VerifierChannel> channel_;
  MaybeOwnedPtr<const TableVerifierFactory<ExtensionFieldElement>> table_verifier_factory_;
  MaybeOwnedPtr<const FriParameters> params_;
  MaybeOwnedPtr<FirstLayerCallback> first_layer_queries_callback_;
  size_t n_layers_;

  std::optional<std::vector<ExtensionFieldElement>> expected_last_layer_;
  std::optional<ExtensionFieldElement> first_eval_point_;
  std::vector<ExtensionFieldElement> eval_points_;
  std::vector<std::unique_ptr<TableVerifier<ExtensionFieldElement>>> table_verifiers_;
  std::vector<uint64_t> query_indices_;
  std::vector<ExtensionFieldElement> query_results_;
};

}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_VERIFIER_H_
