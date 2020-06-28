#ifndef STARKWARE_STARK_OODS_H_
#define STARKWARE_STARK_OODS_H_

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "starkware/stark/composition_oracle.h"

// This file declares the functions that are used for the process of Out Of Domain Sampling - OODS,
// known also as DEEP -  Domain Extension for Eliminating Pretenders.

namespace starkware {
namespace oods {

/*
  Given an evaluation of a composition polynomial of size and degree bound n_breaks * trace_length,
  breaks into n_breaks evaluations of polynomials of degree trace_length. Each evaluation is over a
  new domain. Returns the evaluations and the new domain. See breaker.h for more details.
*/
std::pair<CompositionTrace, Coset> BreakCompositionPolynomial(
    gsl::span<const ExtensionFieldElement> composition_evaluation, size_t n_breaks,
    const Coset& domain);

/*
  Returns an AIR representing the given boundary constraints.
  boundary_constraints is a vector of tuples. Each tuple is single a boundary constraint,
  represented as (index of the column, evaluation point, evaluation value).
*/
std::unique_ptr<Air> CreateBoundaryAir(
    uint64_t trace_length, size_t n_columns,
    std::vector<std::tuple<size_t, ExtensionFieldElement, ExtensionFieldElement>>&&
        boundary_constraints);

/*
  Receives z, a random point from the verifier, and sends the verifier the necessary values it
  needs for OODS:
  * The mask of the original traces evaluated at point z.
  * The broken trace evaluated at point z (broken from the composition polynomial evaluation of the
    original oracle).
  Returns the boundary constraints needed for the rest of the proof.
*/
std::vector<std::tuple<size_t, ExtensionFieldElement, ExtensionFieldElement>> ProveOods(
    ProverChannel* channel, const CompositionOracleProver& original_oracle,
    const CommittedTraceProverBase<ExtensionFieldElement>& composition_trace);

/*
  Sends z, a random point to the prover, and receives from the prover the necessary values we need
  for OODS:
  * The mask of the original traces at point z.
  * The broken trace at point z (broken from the composition polynomial evaluation of the original
  oracle).
  Checks that applying the composition polynomial on the mask values yields the expected composition
  polynomial value, assembled from the broken trace values. Returns the boundary constraints needed
  for the rest of the proof.
*/
std::vector<std::tuple<size_t, ExtensionFieldElement, ExtensionFieldElement>> VerifyOods(
    const EvaluationDomain& evaluation_domain, VerifierChannel* channel,
    const CompositionOracleVerifier& original_oracle, const Coset& composition_eval_domain);

}  // namespace oods
}  // namespace starkware

#endif  // STARKWARE_STARK_OODS_H_
