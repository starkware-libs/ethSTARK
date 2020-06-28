#ifndef STARKWARE_COMMITMENT_SCHEME_COMMITMENT_SCHEME_H_
#define STARKWARE_COMMITMENT_SCHEME_COMMITMENT_SCHEME_H_

#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

/*
  Commitment scheme interfaces.

  Commitment scheme implements two classes :
  1) CommitmentSchemeProver
  2) CommitmentSchemeVerifier

  The implementations expect to use prover and verifier channels to pass commitments and
  decommitments, this allows interactivity in those phases.

  Common notations:

  Element - consecutive buffer of bytes (std::byte). The committed data is regarded by both prover
  and verifier as a vector of elements, although represented in code as a long vector of bytes.
  Verifier queries are interpreted as the indices of elements. Example: Assume
  the required element size is 10 bytes, and the data should include 1024 elements, then the data
  should be a span of 10240 bytes, and element #3 is located at bytes 30-39 (inclusive). Each query
  is a number between 0-1023 (inclusive).

  Segments - In order to provide scalability on the prover-side, the data (aka the sequence of
  elements) is partitioned into segments, where all segments are consecutive subsequences of data,
  all of the same length.

  Note regarding queries:

  When the prover side is asked to prepare a decommitment to a set of queries, the decommitment
  includes all information required to verify integrity of the data in those location with the
  commitment, except for the data itself. In case the data needs to be passed as well, it is not
  done using this interface, rather - one should send this information directly over the channel.
  This is done to allow optimization over communication-complexity (aka argument-length or
  proof-length), where in some cases the verifier can compute parts of the committed data by itself,
  and only needs to verify the integrity of its result with the commitment.
*/
class CommitmentSchemeProver {
 public:
  virtual ~CommitmentSchemeProver() = default;

  /*
    Returns the number of segments.
  */
  virtual size_t NumSegments() const = 0;

  /*
    Segments length measured in elements.
  */
  virtual uint64_t SegmentLengthInElements() const = 0;

  /*
    Methods to feed the commitment-scheme with data to commit on.
  */
  virtual void AddSegmentForCommitment(
      gsl::span<const std::byte> segment_data, size_t segment_index) = 0;

  /*
    Commits to the data by sending the commitment on the channel (may be interactive).
    Method to compute commitment, assuming all data was passed to the commitment-scheme (using
    AddSegmentForCommitment) if not all data was passed the behaviour is undefined.
  */
  virtual void Commit() = 0;

  /*
    Starts the decommitment phase, by passing queries for integrity queries (integrity queries are
    indices for elements which the verifier can compute on its own, but wants to verify that their
    values are consistent with the commitment). The queries are indices of elements from the data
    vector. The function returns a vector of (distinct) indices to elements that should be passed to
    Decommit().
  */
  virtual std::vector<uint64_t> StartDecommitmentPhase(const std::set<uint64_t>& queries) = 0;

  /*
    Decommit to data stored in queried locations, using the channel provided to the constructor (may
    be interactive).
    elements_data is a concatenation of the elements requested by StartDecommitmentPhase().
  */
  virtual void Decommit(gsl::span<const std::byte> elements_data) = 0;
};

class CommitmentSchemeVerifier {
 public:
  virtual ~CommitmentSchemeVerifier() = default;
  virtual void ReadCommitment() = 0;

  /*
    Verifies integrity of parts of the data (elements_to_verify) with the commitment (expected to be
    received through the VerifierChannel on invocation of ReadCommitment). For verification it uses
    the VerifierChannel to receive decommitment (may be interactive) for those data parts.
    The queries the decommitment was generated for must be exactly the set of keys of the
    elements_to_verify mapping, and the value are the expected values of elements in those
    locations.
  */
  virtual bool VerifyIntegrity(
      const std::map<uint64_t, std::vector<std::byte>>& elements_to_verify) = 0;
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_COMMITMENT_SCHEME_H_
