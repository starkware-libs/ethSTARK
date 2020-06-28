#include "starkware/composition_polynomial/neighbors.h"

#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::HasSubstr;

TEST(Neighbors, Iterator) {
  const size_t trace_length = 8;
  const size_t n_trace_columns = 4;
  const size_t n_composition_trace_columns = 4;

  const std::array<std::pair<int64_t, uint64_t>, 5> mask = {
      {{0, 0}, {0, 1}, {1, 6}, {2, 0}, {2, 3}}};

  // Create a random trace.
  Prng prng;
  std::vector<std::vector<BaseFieldElement>> trace;
  std::vector<std::vector<ExtensionFieldElement>> composition_trace;
  trace.reserve(n_trace_columns);
  for (size_t i = 0; i < n_trace_columns; ++i) {
    trace.push_back(prng.RandomFieldElementVector<BaseFieldElement>(trace_length));
  }
  composition_trace.reserve(n_composition_trace_columns);
  for (size_t i = 0; i < n_composition_trace_columns; ++i) {
    composition_trace.push_back(prng.RandomFieldElementVector<ExtensionFieldElement>(trace_length));
  }

  // Evaluate neighbors and test.
  const Neighbors neighbors(
      mask, std::vector<gsl::span<const BaseFieldElement>>(trace.begin(), trace.end()),
      std::vector<gsl::span<const ExtensionFieldElement>>(
          composition_trace.begin(), composition_trace.end()));

  std::vector<std::vector<BaseFieldElement>> result_base;
  std::vector<std::vector<ExtensionFieldElement>> result_ext;
  result_base.reserve(trace_length);
  result_ext.reserve(trace_length);
  for (auto vals : neighbors) {
    result_base.emplace_back(vals.first.begin(), vals.first.end());
    result_ext.emplace_back(vals.second.begin(), vals.second.end());
  }

  const std::vector<std::vector<BaseFieldElement>> expected_result_base = {
      {trace[0][0], trace[1][0], trace[0][2], trace[3][2]},
      {trace[0][1], trace[1][1], trace[0][3], trace[3][3]},
      {trace[0][2], trace[1][2], trace[0][4], trace[3][4]},
      {trace[0][3], trace[1][3], trace[0][5], trace[3][5]},
      {trace[0][4], trace[1][4], trace[0][6], trace[3][6]},
      {trace[0][5], trace[1][5], trace[0][7], trace[3][7]},
      {trace[0][6], trace[1][6], trace[0][0], trace[3][0]},
      {trace[0][7], trace[1][7], trace[0][1], trace[3][1]}};

  const std::vector<std::vector<ExtensionFieldElement>> expected_result_ext = {
      {composition_trace[2][1]}, {composition_trace[2][2]}, {composition_trace[2][3]},
      {composition_trace[2][4]}, {composition_trace[2][5]}, {composition_trace[2][6]},
      {composition_trace[2][7]}, {composition_trace[2][0]}};

  EXPECT_EQ(result_base, expected_result_base);
  EXPECT_EQ(result_ext, expected_result_ext);
}

TEST(Neighbors, InvalidMask) {
  const size_t trace_length = 8;
  const size_t n_columns = 3;
  const std::array<std::pair<int64_t, uint64_t>, 5> mask = {
      {{0, 0}, {0, 1}, {1, 2}, {2, 0}, {2, 3}}};
  std::vector<std::vector<BaseFieldElement>> trace;
  Prng prng;
  trace.reserve(n_columns);
  for (size_t i = 0; i < n_columns; ++i) {
    trace.push_back(prng.RandomFieldElementVector<BaseFieldElement>(trace_length));
  }
  EXPECT_ASSERT(
      Neighbors(
          mask, std::vector<gsl::span<const BaseFieldElement>>(trace.begin(), trace.end()), {}),
      HasSubstr("Too few trace LDE columns provided"));
}

}  // namespace
}  // namespace starkware
