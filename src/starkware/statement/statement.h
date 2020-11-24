#ifndef STARKWARE_STATEMENT_STATEMENT_H_
#define STARKWARE_STATEMENT_STATEMENT_H_

#include <cstddef>
#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/air.h"
#include "starkware/air/trace.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/json.h"
#include "starkware/utils/maybe_owned_ptr.h"
#include "starkware/utils/serialization.h"

namespace starkware {

/*
  Represents a combinatorial statement that the integrity of its computation can be proven using an
  AIR.

  Each subclass can be constructed from the public input (JsonValue) whose contents depend on the
  specific statement.
*/
class Statement {
 public:
  explicit Statement(std::optional<JsonValue> private_input)
      : private_input_(std::move(private_input)) {}
  virtual ~Statement() = default;

  /*
    Generates and returns an AIR for the statement. The AIR is stored as a local member of the
    Statement class.
  */
  virtual const Air& GetAir(bool is_zero_knowledge, size_t n_queries) = 0;

  /*
    Returns the default initial seed to be used in the hash chain. This is obtained from the public
    parameters in some deterministic way.
  */
  virtual const std::vector<std::byte> GetInitialHashChainSeed() const = 0;

  /*
    Returns the private seed, which is used to generate randomness that is known only to the prover
    for the sake of zero knowledge. This is obtained from the private parameters in some
    deterministic way.
  */
  virtual const std::vector<std::byte> GetZeroKnowledgeHashChainSeed() const = 0;

  /*
    Builds and returns a trace for the given private input. The content of the private input depends
    on the specific statement.
  */
  virtual Trace GetTrace(Prng* prng) const = 0;

  /*
    Evaluates the public input according to the private input, and returns a new public input JSON.

    This is mostly used when one wants to prove a statement, but doesn't yet have
    the public input. For this use case, one must set the --fix_public_input flag.
  */
  virtual JsonValue FixPublicInput() = 0;

  /*
    Returns the name of the statement for annotation purposes.
  */
  virtual std::string GetName() const = 0;

  const JsonValue& GetPrivateInput() const {
    ASSERT_RELEASE(private_input_.has_value(), "Missing private input.");
    return private_input_.value();
  }

 protected:
  const std::optional<JsonValue> private_input_;
};

}  // namespace starkware

#endif  // STARKWARE_STATEMENT_STATEMENT_H_
