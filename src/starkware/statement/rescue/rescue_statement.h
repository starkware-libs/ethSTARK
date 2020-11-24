#ifndef STARKWARE_STATEMENT_RESCUE_RESCUE_STATEMENT_H_
#define STARKWARE_STATEMENT_RESCUE_RESCUE_STATEMENT_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "starkware/air/air.h"
#include "starkware/air/rescue/rescue_air.h"
#include "starkware/air/trace.h"
#include "starkware/statement/statement.h"
#include "starkware/utils/json.h"

namespace starkware {

/*
  Public input:
    * Hash output (array of four field elements).
    * Chain length (size_t).
  Private input:
    * Witness (vector of arrays of four field elements).
*/
class RescueStatement : public Statement {
 public:
  using WordT = typename RescueAir::WordT;
  using WitnessT = typename RescueAir::WitnessT;
  explicit RescueStatement(const JsonValue& public_input, std::optional<JsonValue> private_input);

  const Air& GetAir(bool is_zero_knowledge, size_t n_queries) override;

  /*
    Returns the serialization of: ["Rescue hash chain", output, chain_length],
    where chain_length and each of the field elements in output are written as 8 bytes.
  */
  const std::vector<std::byte> GetInitialHashChainSeed() const override;

  /*
    Returns the serialization of: ["Rescue hash chain private seed", witness],
    where each of the field elements in witness is written as 8 bytes.
  */
  const std::vector<std::byte> GetZeroKnowledgeHashChainSeed() const override;

  Trace GetTrace(Prng* prng) const override;

  /*
    An auxiliary function for FixPublicInput(). Given a private input, returns the corresponding
    public input json.
  */
  static JsonValue GetPublicInputJsonValueFromPrivateInput(const JsonValue& private_input);

  /*
    Evaluates the public input according to the private input, and returns a new public input JSON.

    This is mostly used when one wants to prove a statement, but doesn't yet have
    the public input. For this use case, one must set the --fix_public_input flag.
  */
  JsonValue FixPublicInput() override;

  std::string GetName() const override { return "rescue"; }

 private:
  /*
    Processes the JsonValue witness and returns it in the form of WitnessT.
  */
  static WitnessT ParseWitness(const JsonValue& witness);

  /*
    Processes the JsonValue private_input_ and returns it in the form of WitnessT.
  */
  WitnessT GetWitness() const;

  WordT output_;
  uint64_t chain_length_;
  std::unique_ptr<RescueAir> air_;
  // As long as air_ is not initialized, defaults to false.
  bool is_zero_knowledge_ = false;
  std::optional<WitnessT> witness_;
};

}  // namespace starkware

#endif  // STARKWARE_STATEMENT_RESCUE_RESCUE_STATEMENT_H_
