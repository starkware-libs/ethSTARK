#ifndef STARKWARE_AIR_TRACE_H_
#define STARKWARE_AIR_TRACE_H_

#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/fields/extension_field_element.h"

namespace starkware {

/*
  A simple representation of an execution trace of a computation.
  Used to hold the trace and the composition trace.
*/
template <typename FieldElementT>
class TraceBase {
 public:
  explicit TraceBase(std::vector<std::vector<FieldElementT>>&& values) {
    ASSERT_RELEASE(!values.empty(), "Trace cannot be empty.");
    const size_t length = values.at(0).size();
    for (auto& column : values) {
      ASSERT_RELEASE(column.size() == length, "All trace columns must be of the same length.");
      values_.emplace_back(std::move(column));
    }
  }

  /*
    Assumes values is a valid trace.
  */
  static TraceBase CopyFrom(gsl::span<const gsl::span<const FieldElementT>> values) {
    TraceBase trace;
    trace.values_.reserve(values.size());
    for (auto& column : values) {
      trace.values_.emplace_back(column.begin(), column.end());
    }
    return trace;
  }

  /*
    Returns the length of the trace.
  */
  size_t Length() const { return values_[0].size(); }

  /*
    Returns the number of columns in the trace.
  */
  size_t Width() const { return values_.size(); }

  /*
    Consumes the data for the trace and returns it as a vector of columns.
  */
  std::vector<std::vector<FieldElementT>> ConsumeAsColumnsVector() && { return std::move(values_); }

  /*
    Returns a column from the trace by index. This method should only be used for testing.
  */
  const std::vector<FieldElementT>& GetColumn(size_t index) const { return values_[index]; }

  /*
    Sets one cell of the trace to the given value. This method should only be used for testing.
  */
  void SetTraceElementForTesting(size_t column, size_t index, const FieldElementT& field_element) {
    values_[column][index] = field_element;
  }

  /*
    Spaces the trace with random values, making each column slackness_factor times longer.
  */
  void AddZeroKnowledgeSlackness(size_t slackness_factor, Prng* prng) {
    for (auto& column : values_) {
      std::vector<FieldElementT> replacement;
      replacement.reserve(column.size() * slackness_factor);
      for (auto& value : column) {
        replacement.push_back(value);
        for (size_t i = 0; i < slackness_factor - 1; ++i) {
          replacement.push_back(FieldElementT::RandomElement(prng));
        }
      }
      column = replacement;
    }
  }

  /*
    Adds another column, filled with random values.
  */
  void AddZeroKnowledgeExtraColumn(Prng* prng) {
    values_.emplace_back(prng->RandomFieldElementVector<FieldElementT>(values_[0].size()));
  }

 private:
  TraceBase() = default;

  std::vector<std::vector<FieldElementT>> values_;
};

using Trace = TraceBase<BaseFieldElement>;
using CompositionTrace = TraceBase<ExtensionFieldElement>;

}  // namespace starkware

#endif  // STARKWARE_AIR_TRACE_H_
