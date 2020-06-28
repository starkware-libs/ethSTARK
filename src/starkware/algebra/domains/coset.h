#ifndef STARKWARE_ALGEBRA_DOMAINS_COSET_H_
#define STARKWARE_ALGEBRA_DOMAINS_COSET_H_

#include <utility>
#include <vector>

#include "starkware/algebra/fft/multiplicative_group_ordering.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {

/*
  Coset class represents a coset of a cyclic multiplicative subgroup of the multiplicative group of
  the field. An instance of this class is constructed by generating a coset of the given size and
  offset over the given field.
*/
class Coset {
 public:
  Coset(size_t size, const BaseFieldElement& offset)
      : size_(size), generator_(GetSubGroupGenerator(size)), offset_(offset) {
    ASSERT_RELEASE(offset != BaseFieldElement::Zero(), "The offset of a coset cannot be zero.");
  }

  Coset(size_t size, const BaseFieldElement& generator, const BaseFieldElement& offset)
      : size_(size), generator_(generator), offset_(offset) {
    ASSERT_RELEASE(offset != BaseFieldElement::Zero(), "The offset of a coset cannot be zero.");
  }

  size_t Size() const { return size_; }
  const BaseFieldElement& Generator() const { return generator_; }
  const BaseFieldElement& Offset() const { return offset_; }

  /*
    Returns the element at the given index in natural order:
      offset * generator^idx.
  */
  BaseFieldElement At(size_t idx) const { return offset_ * Pow(generator_, idx); }

  /*
    Returns the element at the given index in bit-reversed order:
      offset * generator^bit_reverse(idx).
  */
  BaseFieldElement AtBitReversed(size_t idx) const { return At(BitReverse(idx, SafeLog2(size_))); }

  /*
    Returns the first n elements of the coset:
      offset, offset * generator, offset * generator^2, ..., offset * generator^(n-1).
  */
  std::vector<BaseFieldElement> GetFirstElements(size_t n_elements) const {
    ASSERT_RELEASE(n_elements <= size_, "The number of elements must not exceed coset size.");
    std::vector<BaseFieldElement> res;
    BaseFieldElement point = offset_;
    res.reserve(n_elements);
    for (size_t i = 0; i < n_elements; ++i) {
      res.push_back(point);
      point *= generator_;
    }
    return res;
  }

  /*
    Returns the elements of the coset in natural or bit-reversed order.
  */
  std::vector<BaseFieldElement> GetElements(MultiplicativeGroupOrdering order) const {
    std::vector<BaseFieldElement> elements_in_natural_order = GetFirstElements(size_);
    return order == MultiplicativeGroupOrdering::kBitReversedOrder
               ? BitReverseVector<BaseFieldElement>(elements_in_natural_order)
               : elements_in_natural_order;
  }

 private:
  size_t size_;
  BaseFieldElement generator_;
  BaseFieldElement offset_;
};

}  // namespace starkware

#endif  // STARKWARE_ALGEBRA_DOMAINS_COSET_H_
