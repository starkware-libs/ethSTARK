#ifndef STARKWARE_ALGEBRA_DOMAINS_EVALUATION_DOMAIN_H_
#define STARKWARE_ALGEBRA_DOMAINS_EVALUATION_DOMAIN_H_

#include <utility>
#include <vector>

#include "starkware/algebra/domains/coset.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"

namespace starkware {

/*
  Represents the evaluation domain.
  Denote the trace domain group by G. The evaluation domain, D, is a coset of an extension of that
  group, whose size is n_cosets times the size of the trace domain.
  The offset of the coset D is the generator of the field's multiplicative group. In particular, G
  and D are disjoint.
*/
class EvaluationDomain {
 public:
  /*
    Initializes an evaluation domain of size trace_size * n_cosets, with trace domain size
    trace_size.
    trace_size and n_cosets must be powers of two.
  */
  EvaluationDomain(size_t trace_size, size_t n_cosets);

  /*
    Returns the generator of the trace domain.
  */
  const BaseFieldElement& TraceGenerator() const { return trace_group_.Generator(); }

  /*
    Returns the number of cosets.
  */
  size_t NumCosets() const { return cosets_offsets_.size(); }

  /*
    Returns the offsets of the cosets.
  */
  const std::vector<BaseFieldElement>& CosetOffsets() const { return cosets_offsets_; }

  /*
    Returns the trace domain.
  */
  const Coset& TraceDomain() const { return trace_group_; }

  /*
    Returns the size of the trace domain.
  */
  size_t TraceSize() const { return trace_group_.Size(); }

  /*
    Returns the size of the evaluation domain.
  */
  size_t Size() const { return trace_group_.Size() * cosets_offsets_.size(); }

  /*
    Returns the group_index element inside the coset_index coset.
  */
  BaseFieldElement ElementByIndex(size_t coset_index, size_t group_index) const;

 private:
  Coset trace_group_;
  // A list of offsets such that the disjoint union of the elements of offset * <trace_generator>
  // is equal to the elements of the evaluation domain.
  std::vector<BaseFieldElement> cosets_offsets_;
};

}  // namespace starkware

#endif  // STARKWARE_ALGEBRA_DOMAINS_EVALUATION_DOMAIN_H_
