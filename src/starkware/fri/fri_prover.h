#ifndef STARKWARE_FRI_FRI_PROVER_H_
#define STARKWARE_FRI_FRI_PROVER_H_

#include <functional>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "starkware/fri/fri_committed_layer.h"
#include "starkware/fri/fri_details.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

/*
  Executes the FRI protocol to prove that a given witness is of degree smaller than a given degree
  bound.
*/
class FriProver {
 public:
  using FirstLayerCallback = std::function<void(const std::vector<uint64_t>& queries)>;

  /*
    Constructs a FRI prover.
    * channel is a ProverChannel used for interaction.
    * table_prover_factory is a function that gets the size of the data for commitment, and creates
      a TableProver for committing and decommitting a 2 dimensional array of field elements.
    * params are the FRI parameters for the FRI prover.
    * witness is the polynomial for which the degree bound should hold.
    * first_layer_queries_callback is a callback function that will be called once, with the indices
      of the queries for the first layer. This callback is responsible for:
      1. Sending enough information to the verifier so that it will be able to compute the values at
      those indices.
      2. Sending the relevant decommitment to allow the verifier to check their correctness.
  */
  FriProver(
      MaybeOwnedPtr<ProverChannel> channel,
      MaybeOwnedPtr<TableProverFactory<ExtensionFieldElement>> table_prover_factory,
      MaybeOwnedPtr<FriParameters> params, std::vector<ExtensionFieldElement>&& witness,
      MaybeOwnedPtr<FirstLayerCallback> first_layer_queries_callback);

  /*
    Runs the FRI protocol.
  */
  void ProveFri();

 private:
  /*
    The commitment phase of the ProveFri sequence is assumed to already have the witness available
    to it as the first layer in the class's committed_layers_ variable, and use it to:
      1. Fill the rest of layers, receiving randomness over the channel to compute the next layer.
      2. Fill the table provers in the table_provers_ class variable, one for each layer, which are
         used to commit on layers, and later on (though not in this phase) authenticate commitments.
    The function returns the last FriLayer.
  */
  MaybeOwnedPtr<const FriLayer> CommitmentPhase();

  /*
    Generates the next FriLayer.
    Done by creating fri_step proxy layers and returning the last one of them.
  */
  MaybeOwnedPtr<const FriLayer> CreateNextFriLayer(
      MaybeOwnedPtr<const FriLayer> current_layer, size_t fri_step, size_t* basis_index);

  /*
    Sends the coefficients of the polynomial, f(x), of the last FRI layer (which is expected to be
    of degree < last_layer_degree_bound) over the channel. Note: the coefficients are computed
    without taking the basis offset into account (that is, the first element of the last layer will
    always be f(1)).
  */
  void SendLastLayer(MaybeOwnedPtr<const FriLayer>&& last_layer);

  // Data:
  MaybeOwnedPtr<ProverChannel> channel_;
  MaybeOwnedPtr<TableProverFactory<ExtensionFieldElement>> table_prover_factory_;
  MaybeOwnedPtr<FriParameters> params_;
  std::vector<ExtensionFieldElement> witness_;
  // Number of FRI layers (not including skipped layers).
  size_t n_layers_;
  // Committed layers that are ready for decommitment.
  std::vector<MaybeOwnedPtr<FriCommittedLayer>> committed_layers_;
};

}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_PROVER_H_
