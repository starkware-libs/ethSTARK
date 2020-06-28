#ifndef STARKWARE_COMPOSITION_POLYNOMIAL_NEIGHBORS_H_
#define STARKWARE_COMPOSITION_POLYNOMIAL_NEIGHBORS_H_

#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

/*
  This class is used to iterate over the induced mask values of given traces.
*/
class Neighbors {
 public:
  /*
    Constructs a Neighbors instance.
    trace_lde_coset and composition_trace_lde_coset should refer to one coset in the LDE of the
    trace, and MUST be kept alive as long the iterator is alive. mask is a list of pairs (relative
    row, column), as defined by an AIR. trace_lde_coset is a list of columns representing the LDE of
    the trace in some coset, in a natural order.
  */
  Neighbors(
      gsl::span<const std::pair<int64_t, uint64_t>> mask,
      gsl::span<const gsl::span<const BaseFieldElement>> trace_lde_coset,
      gsl::span<const gsl::span<const ExtensionFieldElement>> composition_trace_lde_coset);

  // Disable copy-constructor and operator= since end_ refers to this.
  Neighbors(const Neighbors& other) = delete;
  Neighbors& operator=(const Neighbors& other) = delete;
  Neighbors(Neighbors&& other) noexcept = delete;
  Neighbors& operator=(Neighbors&& other) noexcept = delete;

  ~Neighbors() = default;

  class Iterator {
   public:
    Iterator(const Neighbors* parent, size_t idx);

    bool operator==(const Iterator& other) const {
      ASSERT_DEBUG(
          parent_ == other.parent_, "Comparing iterators with different parent_ is not allowed.");
      return idx_ == other.idx_;
    }
    bool operator!=(const Iterator& other) const { return !((*this) == other); }

    Iterator& operator+=(size_t offset) {
      idx_ += offset;
      return *this;
    }

    Iterator& operator++() {
      idx_++;
      return *this;
    }

    /*
      Returns the values of the neighbors.
      Calling operator*() twice is not recommended as it will copy the values twice.
      Returns spans for an internal storage of the iterator, which is invalidated once operator++()
      is called or the iterator is destroyed.
    */
    std::pair<gsl::span<const BaseFieldElement>, gsl::span<const ExtensionFieldElement>>
    operator*();

   private:
    /*
      Pointer to the corresponding Neighbors instance. Used for retreiving the mask and traces.
    */
    const Neighbors* parent_;

    /*
      Index of the current point.
    */
    size_t idx_ = 0;

    /*
      Pre-allocated space for neighbor values.
    */
    std::vector<BaseFieldElement> neighbors_;
    std::vector<ExtensionFieldElement> composition_neighbors_;
  };

  /*
    Note: Destroying this instance will invalidate the iterator.
  */
  Iterator begin() const { return Iterator(this, 0); }  // NOLINT: invalid case style.

  const Iterator& end() const { return end_; }  // NOLINT: invalid case style.

  uint64_t CosetSize() const { return coset_size_; }

 private:
  const std::vector<std::pair<int64_t, uint64_t>> mask_;
  const uint64_t coset_size_;

  /*
    Precomputed value to allow computing (x % coset_size_) using an & operation.
  */
  const size_t neighbor_wraparound_mask_;
  const std::vector<gsl::span<const BaseFieldElement>> trace_lde_coset_;
  const std::vector<gsl::span<const ExtensionFieldElement>> composition_trace_lde_coset_;

  /*
    Keep one instance of the iterator at the end, so that calls to end() will not allocate memory.
  */
  const Iterator end_;
};

}  // namespace starkware

#endif  // STARKWARE_COMPOSITION_POLYNOMIAL_NEIGHBORS_H_
