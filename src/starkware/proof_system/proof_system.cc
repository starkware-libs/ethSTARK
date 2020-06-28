#include "starkware/proof_system/proof_system.h"

#include "glog/logging.h"

#include "starkware/error_handling/error_handling.h"

namespace starkware {

bool FalseOnError(const std::function<void()>& func) {
  try {
    func();
    return true;
  } catch (const StarkwareException& e) {
    LOG(ERROR) << e.Message();
    return false;
  }
}

}  // namespace starkware
