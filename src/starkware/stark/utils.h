#ifndef STARKWARE_STARK_UTILS_H_
#define STARKWARE_STARK_UTILS_H_

#include "starkware/channel/prover_channel.h"
#include "starkware/commitment_scheme/table_prover.h"

namespace starkware {

template <typename FieldElementT>
TableProverFactory<FieldElementT> GetTableProverFactory(ProverChannel* channel);

}  // namespace starkware

#include "starkware/stark/utils.inl"

#endif  // STARKWARE_STARK_UTILS_H_
