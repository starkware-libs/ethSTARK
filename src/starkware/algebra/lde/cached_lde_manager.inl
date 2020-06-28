#include "starkware/algebra/lde/cached_lde_manager.h"

#include "starkware/utils/bit_reversal.h"

namespace starkware {

template <typename FieldElementT>
const typename CachedLdeManager<FieldElementT>::LdeCacheEntry*
CachedLdeManager<FieldElementT>::EvalOnCoset(uint64_t coset_index) {
  ASSERT_RELEASE(done_adding_, "Must call FinalizeAdding() before calling EvalOnCoset().");
  ASSERT_RELEASE(coset_index < cache_.size(), "Coset index out of bounds.");

  // If a cached entry exists, return it.
  if (cache_[coset_index].has_value()) {
    return &*cache_[coset_index];
  }

  ASSERT_RELEASE(
      lde_manager_.HasValue(),
      "Cannot evaluate new values after FinalizeEvaluations() was called.");

  // Allocate a new cache entry.
  cache_[coset_index] = AllocateEntry();
  LdeCacheEntry* storage = &*cache_[coset_index];
  ASSERT_RELEASE(storage != nullptr, "Invalid storage");

  // Evaluate on columns, store result in the new cache entry.
  const BaseFieldElement& coset_offset = coset_offsets_.at(coset_index);
  lde_manager_->EvalOnCoset(
      coset_offset, std::vector<gsl::span<FieldElementT>>(storage->begin(), storage->end()));

  // Return a pointer to the cache entry containing the result.
  return storage;
}

template <typename FieldElementT>
void CachedLdeManager<FieldElementT>::EvalAtPoints(
    gsl::span<const std::pair<uint64_t, uint64_t>> coset_and_point_indices,
    gsl::span<const gsl::span<FieldElementT>> outputs) {
  ASSERT_RELEASE(done_adding_, "Must call FinalizeAdding() before calling EvalAtPoints().");
  ASSERT_RELEASE(outputs.size() == n_columns_, "Wrong number of output columns.");
  for (size_t column_index = 0; column_index < n_columns_; ++column_index) {
    ASSERT_RELEASE(
        coset_and_point_indices.size() == outputs[column_index].size(),
        "Number of output points is different than number of input points.");
  }

  // Look up values in cache and fill the outputs.
  for (size_t i = 0; i < coset_and_point_indices.size(); ++i) {
    const auto& [coset_index, point_index] = coset_and_point_indices[i];

    // Check that the requested coset is cached and the point index is valid.
    ASSERT_RELEASE(
        cache_[coset_index].has_value(), "EvalAtPoints requested a coset that is not cached!");
    ASSERT_RELEASE(point_index < domain_size_, "Point index out of range.");

    // Bit-reverse point_index if needed.
    const uint64_t fixed_point_index =
        eval_in_natural_order_ ? BitReverse(point_index, SafeLog2(domain_size_)) : point_index;

    // Copy cached values.
    for (size_t column_index = 0; column_index < n_columns_; ++column_index) {
      outputs.at(column_index).at(i) = (*cache_[coset_index])[column_index].at(fixed_point_index);
    }
  }
}

template <typename FieldElementT>
void CachedLdeManager<FieldElementT>::EvalAtPointsNotCached(
    size_t column_index, gsl::span<const ExtensionFieldElement> points,
    gsl::span<ExtensionFieldElement> output) {
  ASSERT_RELEASE(
      lde_manager_.HasValue(),
      "Cannot evaluate new values after FinalizeEvaluations() was called.");
  lde_manager_->EvalAtPoints(column_index, points, output);
}

template <typename FieldElementT>
typename CachedLdeManager<FieldElementT>::LdeCacheEntry
CachedLdeManager<FieldElementT>::AllocateEntry() const {
  LdeCacheEntry entry;
  const uint64_t coset_size = domain_size_;
  entry.reserve(n_columns_);
  for (size_t i = 0; i < n_columns_; ++i) {
    entry.push_back(FieldElementT::UninitializedVector(coset_size));
  }
  return entry;
}

template <typename FieldElementT>
void CachedLdeManager<FieldElementT>::FinalizeEvaluations() {
  ASSERT_RELEASE(done_adding_, "Must call FinalizeAdding() before calling FinalizeEvaluations().");
  lde_manager_.reset();
}

}  // namespace starkware
