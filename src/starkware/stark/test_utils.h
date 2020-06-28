#ifndef STARKWARE_STARK_TEST_UTILS_H_
#define STARKWARE_STARK_TEST_UTILS_H_

#include <memory>
#include <utility>

#include "starkware/commitment_scheme/commitment_scheme_builder.h"
#include "starkware/commitment_scheme/packaging_commitment_scheme.h"
#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/commitment_scheme/table_verifier_impl.h"

namespace starkware {

template <typename FieldElementT>
std::unique_ptr<TableVerifier<FieldElementT>> MakeTableVerifier(
    uint64_t n_rows, uint64_t n_columns, VerifierChannel* channel) {
  auto commitment_scheme_verifier =
      MakeCommitmentSchemeVerifier(n_columns * FieldElementT::SizeInBytes(), n_rows, channel);

  return std::make_unique<TableVerifierImpl<FieldElementT>>(
      n_columns, UseMovedValue(std::move(commitment_scheme_verifier)), channel);
}

}  // namespace starkware

#endif  // STARKWARE_STARK_TEST_UTILS_H_
