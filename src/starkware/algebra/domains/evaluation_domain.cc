#include "starkware/algebra/domains/evaluation_domain.h"

#include "starkware/algebra/field_operations.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {

namespace {

std::vector<BaseFieldElement> GetCosetsOffsets(
    const size_t n_cosets, const BaseFieldElement& domain_generator,
    const BaseFieldElement& common_offset) {
  // Define result vector.
  std::vector<BaseFieldElement> result;
  result.reserve(n_cosets);

  // Compute the offsets vector.
  BaseFieldElement offset = common_offset;
  result.emplace_back(offset);
  for (size_t i = 1; i < n_cosets; ++i) {
    offset *= domain_generator;
    result.emplace_back(offset);
  }

  return result;
}

}  // namespace

EvaluationDomain::EvaluationDomain(size_t trace_size, size_t n_cosets)
    : trace_group_(trace_size, BaseFieldElement::One()) {
  ASSERT_RELEASE(trace_size > 1, "trace_size must be > 1.");
  ASSERT_RELEASE(IsPowerOfTwo(trace_size), "trace_size must be a power of 2.");
  ASSERT_RELEASE(IsPowerOfTwo(n_cosets), "n_cosets must be a power of 2.");
  cosets_offsets_ = GetCosetsOffsets(
      n_cosets, GetSubGroupGenerator(trace_size * n_cosets), BaseFieldElement::Generator());
}

BaseFieldElement EvaluationDomain::ElementByIndex(size_t coset_index, size_t group_index) const {
  ASSERT_RELEASE(coset_index < cosets_offsets_.size(), "Coset index is out of range.");
  ASSERT_RELEASE(group_index < trace_group_.Size(), "Group index is out of range.");

  group_index = BitReverse(group_index, SafeLog2(trace_group_.Size()));
  const BaseFieldElement point = Pow(trace_group_.Generator(), group_index);
  const BaseFieldElement offset = cosets_offsets_[coset_index];

  return offset * point;
}

}  // namespace starkware
