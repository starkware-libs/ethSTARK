#ifndef STARKWARE_COMPOSITION_POLYNOMIAL_PERIODIC_COLUMN_H_
#define STARKWARE_COMPOSITION_POLYNOMIAL_PERIODIC_COLUMN_H_

#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/lde/lde_manager.h"

namespace starkware {

/*
  Represents a polynomial whose evaluation on a given coset is periodic with a given period.
  This can be used to simulate public columns (known both to the prover and the verifier) where the
  data of the column is periodic with a relatively small period. For example, round constants that
  appear in a hash function and repeat every invocation.

  Example usage:
    PeriodicColumn p = ...;
    auto coset_eval = p.GetCoset(...);
    ParallelFor(..., {
      auto it = coset_eval.begin();
      // Do stuff with iterator, safely
    }).
*/
class PeriodicColumn {
 public:
  /*
    Constructs a PeriodicColumn whose evaluation on the trace domain is composed of repetitions of
    the given values. Namely, f(trace_generator^i) = values[i % values.size()].
  */
  PeriodicColumn(
      gsl::span<const BaseFieldElement> values, uint64_t trace_size, size_t slackness_factor = 1);

  /*
    Returns the evaluation of the interpolation polynomial at a given point.
  */
  template <typename FieldElementT>
  FieldElementT EvalAtPoint(const FieldElementT& x) const;

  // Forward declaration.
  class CosetEvaluation;

  /*
    Returns an iterator that computes the polynomial on a coset of the same size as the trace.
  */
  CosetEvaluation GetCoset(const BaseFieldElement& start_point, size_t coset_size) const;

 private:
  /*
    The period of the column with respect to the trace.
    Note that
      period_in_trace_ = values.size().
  */
  const uint64_t period_in_trace_;

  /*
    The size of the coset divided by the length of the period.
  */
  const size_t n_copies_;

  /*
    The lde_manager of the column. This should be treated as a polynomial in x^{n_copies}.
  */
  LdeManager<BaseFieldElement> lde_manager_;
};

/*
  Represents an efficient evaluation of the periodic column on a coset. Can spawn thin iterators to
  the evaluation, which are thread-safe.
*/
class PeriodicColumn::CosetEvaluation {
 public:
  explicit CosetEvaluation(std::vector<BaseFieldElement> values)
      : values_(std::move(values)), index_mask_(values_.size() - 1) {
    ASSERT_RELEASE(IsPowerOfTwo(values_.size()), "values must be of size which is a power of two.");
  }

  class Iterator {
   public:
    Iterator(const CosetEvaluation* parent, uint64_t index, const uint64_t index_mask)
        : parent_(parent), index_(index), index_mask_(index_mask) {}

    Iterator& operator++() {
      index_ = (index_ + 1) & index_mask_;
      return *this;
    }

    Iterator operator+(uint64_t offset) const {
      return Iterator(parent_, (index_ + offset) & index_mask_, index_mask_);
    }

    BaseFieldElement operator*() const { return parent_->values_[index_]; }

   private:
    const CosetEvaluation* parent_;
    uint64_t index_;
    const uint64_t index_mask_;
  };

  Iterator begin() const { return Iterator(this, 0, index_mask_); }  // NOLINT

 private:
  const std::vector<BaseFieldElement> values_;
  const uint64_t index_mask_;
};

}  // namespace starkware

#include "starkware/composition_polynomial/periodic_column.inl"

#endif  // STARKWARE_COMPOSITION_POLYNOMIAL_PERIODIC_COLUMN_H_
