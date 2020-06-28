#ifndef STARKWARE_ALGEBRA_LDE_CACHED_LDE_MANAGER_H_
#define STARKWARE_ALGEBRA_LDE_CACHED_LDE_MANAGER_H_

#include <optional>
#include <utility>
#include <vector>

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/algebra/lde/lde_manager.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

/*
  A wrapper class for LdeManager. Caches EvalOnCoset calls.
  Cached results are used for future EvalOnCoset calls as well as for EvalAtPoints function calls.
*/
template <typename FieldElementT>
class CachedLdeManager {
  /*
    LdeCacheEntry is a vector of n_columns_ vectors. Each FieldElementT vector is the same size as
    the trace domain. The first LdeCacheEntry index specifies the column, the second index specifies
    the entry in the coset.
  */
  using LdeCacheEntry = std::vector<std::vector<FieldElementT>>;

 public:
  CachedLdeManager(
      MaybeOwnedPtr<LdeManager<FieldElementT>> lde_manager,
      std::vector<BaseFieldElement>&& coset_offsets)
      : lde_manager_(std::move(lde_manager)),
        coset_offsets_(std::move(coset_offsets)),
        eval_in_natural_order_(lde_manager_->IsEvalNaturallyOrdered()),
        domain_size_(lde_manager_->GetDomainSize()),
        cache_(coset_offsets_.size()) {
    ASSERT_RELEASE(!coset_offsets_.empty(), "At least one coset offset is required.");
  }

  /*
    Adds a new evaluation to the underlying LdeManager.
  */
  void AddEvaluation(std::vector<FieldElementT>&& evaluation) {
    ASSERT_RELEASE(!done_adding_, "Cannot call AddEvaluation after EvalOnCoset.");
    lde_manager_->AddEvaluation(std::move(evaluation));
    ++n_columns_;
  }

  /*
    Adds a new evaluation to the underlying LdeManager.
  */
  void AddEvaluation(gsl::span<const FieldElementT> evaluation) {
    ASSERT_RELEASE(!done_adding_, "Cannot call AddEvaluation after EvalOnCoset.");
    lde_manager_->AddEvaluation(evaluation);
    ++n_columns_;
  }

  /*
    Returns a pointer to the coset evaluation cache.
    If not yet cached, the entire coset is evaluated on, cached, and then a pointer to the cache is
    returned.
  */
  const LdeCacheEntry* EvalOnCoset(uint64_t coset_index);

  /*
    Evaluates all columns at the given cosets and points. Takes pairs of (coset_index, point_index).
    Note: this is a cached version, all requested cosets must already be cached.
  */
  void EvalAtPoints(
      gsl::span<const std::pair<uint64_t, uint64_t>> coset_and_point_indices,
      gsl::span<const gsl::span<FieldElementT>> outputs);

  /*
    Evaluates all columns at the given points.
    Note: this function uses the underlying LdeManager's EvalAtPoints directly, without caching.
  */
  void EvalAtPointsNotCached(
      size_t column_index, gsl::span<const ExtensionFieldElement> points,
      gsl::span<ExtensionFieldElement> output);

  /*
    Indicates that no new uncached evaluations will occur anymore.
    This includes calls to EvalAtPointsNotCached() and EvalAtPoints() on a new uncached coset.
  */
  void FinalizeEvaluations();

  /*
    Indicates AddEvaluation() will not be called anymore.
  */
  void FinalizeAdding() { done_adding_ = true; }

  /*
    Returns the number of columns in CachedLdeManager.
    Can be called only after FinalizeAdding() was invoked (and done_adding_ equals true), in order
    to ensure the number of columns will not change.
  */
  size_t NumColumns() const {
    ASSERT_RELEASE(done_adding_, "NumColumns() must be called after calling FinalizeAdding().");
    return n_columns_;
  }

  /*
    Returns true if the evaluation is naturally ordered, false if it is bit-reversed ordered.
  */
  bool IsEvalNaturallyOrdered() const { return eval_in_natural_order_; }

 private:
  /*
    Allocates a new coset entry, ready to be filled.
  */
  LdeCacheEntry AllocateEntry() const;

  MaybeOwnedPtr<LdeManager<FieldElementT>> lde_manager_;
  const std::vector<BaseFieldElement> coset_offsets_;
  const bool eval_in_natural_order_;
  const uint64_t domain_size_;
  bool done_adding_ = false;
  size_t n_columns_ = 0;

  /*
    Cache entries. Each LdeCacheEntry represents a coset.
    Indices are: coset_index, column_index, point_index.

    Vector items are optional, where nullopt means that the coset was not cached yet.
  */
  std::vector<std::optional<LdeCacheEntry>> cache_;
};

}  // namespace starkware

#include "starkware/algebra/lde/cached_lde_manager.inl"

#endif  // STARKWARE_ALGEBRA_LDE_CACHED_LDE_MANAGER_H_
