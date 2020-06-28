#ifndef STARKWARE_PROOF_SYSTEM_PROOF_SYSTEM_H_
#define STARKWARE_PROOF_SYSTEM_PROOF_SYSTEM_H_

#include <functional>

namespace starkware {

/*
  Runs a given function. If the function throws a StarkwareException, returns false and logs the
  error. Otherwise, returns true.
*/
bool FalseOnError(const std::function<void()>& func);

}  // namespace starkware

#endif  // STARKWARE_PROOF_SYSTEM_PROOF_SYSTEM_H_
