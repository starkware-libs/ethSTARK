#ifndef STARKWARE_STARK_STARK_H_
#define STARKWARE_STARK_STARK_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "starkware/air/air.h"
#include "starkware/air/trace.h"
#include "starkware/algebra/domains/coset.h"
#include "starkware/algebra/domains/evaluation_domain.h"
#include "starkware/algebra/lde/cached_lde_manager.h"
#include "starkware/algebra/lde/lde_manager.h"
#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/table_prover.h"
#include "starkware/commitment_scheme/table_prover_impl.h"
#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/composition_polynomial/composition_polynomial.h"
#include "starkware/fri/fri_parameters.h"
#include "starkware/stark/committed_trace.h"
#include "starkware/stark/composition_oracle.h"
#include "starkware/utils/json.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

struct StarkParameters {
  StarkParameters(
      size_t n_evaluation_domain_cosets, size_t trace_length, MaybeOwnedPtr<const Air> air,
      MaybeOwnedPtr<FriParameters> fri_params);

  size_t TraceLength() const { return evaluation_domain.TraceSize(); }
  size_t NumCosets() const { return evaluation_domain.NumCosets(); }
  size_t NumColumns() const { return (air->NumColumns()); }

  static StarkParameters FromJson(const JsonValue& json, MaybeOwnedPtr<const Air> air);

  EvaluationDomain evaluation_domain;
  Coset composition_eval_domain;

  MaybeOwnedPtr<const Air> air;
  MaybeOwnedPtr<FriParameters> fri_params;
};

struct StarkProverConfig {
  /*
    Evaluation of composition polynomial on the coset is split to different tasks of size
    constraint_polynomial_task_size each to allow multithreading.
    The larger the task size the lower the amortized threading overhead but this can also affect the
    fragmentation effect in case the number of tasks doesn't divide the coset by a multiple of the
    number of threads in the thread pool.
  */
  uint64_t constraint_polynomial_task_size;

  static StarkProverConfig Default() {
    return {
        /*constraint_polynomial_task_size=*/256,
    };
  }

  static StarkProverConfig FromJson(const JsonValue& json);
};

class StarkProver {
 public:
  StarkProver(
      MaybeOwnedPtr<ProverChannel> channel,
      MaybeOwnedPtr<TableProverFactory<BaseFieldElement>> base_table_prover_factory,
      MaybeOwnedPtr<TableProverFactory<ExtensionFieldElement>> extension_table_prover_factory,
      MaybeOwnedPtr<const StarkParameters> params, MaybeOwnedPtr<const StarkProverConfig> config)
      : channel_(std::move(channel)),
        base_table_prover_factory_(std::move(base_table_prover_factory)),
        extension_table_prover_factory_(std::move(extension_table_prover_factory)),
        params_(std::move(params)),
        config_(std::move(config)) {}

  /*
    Implements the STARK prover side of the protocol given a trace, a constraint system defined in
    an AIR (to verify the Trace's validity), and a set of protocol parameters defining attributes
    such as the required soundness. The prover uses a commitment scheme factory for generating
    commitments and decommitments and a prover channel for sending proof elements and receiving
    field elements from the simulated verifier. The STARK prover uses a Low Degree Test (LDT) prover
    as its main proof engine. The LDT used is FRI.

    The main STARK prover steps are:
    - Perform a Low Degree Extension (LDE) on each column of the trace.
    - Concatenate the LDEs of the columns to form a table (the "main trace"), and commit
      to it.
    - Get a set of random coefficients from the verifier required to create a random linear
      combination of the AIR constraints, called the Constraint Polynomial, of a degree less than
      degree max_constraint_deg.
    - Define the composition of the Constraint Polynomial over the polynomials represented by the
      "main trace" columns as the Composition Polynomial, which is of a degree less than
      comp_deg = max_constraint_deg * "main trace"_column_length.
    - Evaluate the Composition Polynomial on max_constraint_deg cosets, and break the evaluation
      into max_constraint_deg "broken" evaluations on a single coset.
    - Perform a LDE on each of the "broken" evaluations.
    - Concatenate the LDEs of the "broken" evaluations to form a table (call it the "composition
      trace"), and commit to it.
    - Concatenate the original trace with the composition trace into the "final table" and create
      new constraints on this table.
      This is done via Out Of Domain Sampling (OODS). A random field element Z is sent to the prover
      from the verifier, and the prover sends back the entire mask evaluated at Z, and the broken
      polynomials evaluated at Z. The new boundary constraints verify consistency of the elements
      sent by the prover (to check the prover sent the correct evaluations). The boundary
      constraints also check that the original polynomials are of low degree.
    - With the new constraints in hand, create a new composition polynomial of
      "main trace"_column_length degree.
    - Create a virtual oracle (FRI top layer), which decommits queries of the new composition
      polynomial using the previous commitments.
    - Run a LDT prover (e.g. FRI) to prove that the virtual oracle is of the desired low degree,
      also implying low degreeness of all the above polynomials.
  */
  void ProveStark(Trace&& trace);

 private:
  void PerformLowDegreeTest(const CompositionOracleProver& oracle);

  /*
    Creates a CommittedTraceProver from a trace.
    Parameters:
    * trace: The trace to commit on.
    * trace_domain: The domain on which the trace is defined.
    * bit_reverse: Whether the trace should be bit-reversed prior to committing.
  */
  template <typename FieldElementT>
  CommittedTraceProver<FieldElementT> CommitOnTrace(
      TraceBase<FieldElementT>&& trace, const Coset& trace_domain, bool bit_reverse,
      const std::string& profiling_text);

  CompositionOracleProver OutOfDomainSamplingProve(CompositionOracleProver original_oracle);

  /*
    Validates that the given arguments regarding the trace size are the same as in params_.
  */
  void ValidateTraceSize(size_t n_rows, size_t n_columns);

  MaybeOwnedPtr<ProverChannel> channel_;
  MaybeOwnedPtr<TableProverFactory<BaseFieldElement>> base_table_prover_factory_;
  MaybeOwnedPtr<TableProverFactory<ExtensionFieldElement>> extension_table_prover_factory_;
  MaybeOwnedPtr<const StarkParameters> params_;
  MaybeOwnedPtr<const StarkProverConfig> config_;
};

class StarkVerifier {
 public:
  StarkVerifier(
      MaybeOwnedPtr<VerifierChannel> channel,
      MaybeOwnedPtr<const TableVerifierFactory<BaseFieldElement>> base_table_verifier_factory,
      MaybeOwnedPtr<const TableVerifierFactory<ExtensionFieldElement>>
          extension_table_verifier_factory,
      MaybeOwnedPtr<const StarkParameters> params)
      : channel_(std::move(channel)),
        base_table_verifier_factory_(std::move(base_table_verifier_factory)),
        extension_table_verifier_factory_(std::move(extension_table_verifier_factory)),
        params_(std::move(params)) {}

  /*
    Implements the STARK verifier side of the protocol given constraint system defined in an AIR (to
    verify the Trace) and a set of protocol parameters definition attributes such as the required
    soundness. The verifier uses a commitment scheme factory for requesting commitments and
    decommitments, and a verifier channel for receiving proof elements from the simulated prover.
    The STARK verifier uses a Low Degree Test (LDT) verifier as its main verification engine. The
    LDT used is FRI.

    The main STARK verifier steps are:
    - Receive a commitment on the trace generated by the prover.
    - Send a set of random coefficients to the prover required to create a random linear combination
      of the AIR constraints, called the Constraint Polynomial of a degree less than degree
      max_constraint_deg.
    - Define the composition of the Constraint Polynomial over the polynomials represented by the
      "main trace" columns as the Composition Polynomial, which is of a degree less than
      comp_deg = max_constraint_deg * "main trace"_column_length.
    - Receive a commitment on the max_constraint_deg polynomials representing the broken composition
      polynomial.
    - Perform OODS - send a random field element Z to the prover, and receive the entire mask
      evaluated at Z, and the max_constraint_deg polynomials representing the broken composition
      polynomial evaluated at Z.
    - Calculate the new AIR constraints and new boundary constraints derived from the OODS in order
      to calculate points of a virtual oracle (FRI top layer).
    - Run a LDT verifier (e.g. FRI) to verify that the virtual oracle is of the desired low degree.

    In case verification fails, an exception is thrown.
  */
  void VerifyStark();

 private:
  /*
    Reads the trace's commitment.
  */
  CommittedTraceVerifier<BaseFieldElement> ReadTraceCommitment(size_t n_columns);

  void PerformLowDegreeTest(const CompositionOracleVerifier& oracle);

  CompositionOracleVerifier OutOfDomainSamplingVerify(CompositionOracleVerifier original_oracle);

  MaybeOwnedPtr<VerifierChannel> channel_;
  MaybeOwnedPtr<const TableVerifierFactory<BaseFieldElement>> base_table_verifier_factory_;
  MaybeOwnedPtr<const TableVerifierFactory<ExtensionFieldElement>>
      extension_table_verifier_factory_;
  MaybeOwnedPtr<const StarkParameters> params_;

  std::unique_ptr<CompositionPolynomial> composition_polynomial_;
};

}  // namespace starkware

#endif  // STARKWARE_STARK_STARK_H_
