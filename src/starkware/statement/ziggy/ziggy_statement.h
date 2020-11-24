#ifndef STARKWARE_STATEMENT_ZIGGY_ZIGGY_STATEMENT_H_
#define STARKWARE_STATEMENT_ZIGGY_ZIGGY_STATEMENT_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "starkware/air/air.h"
#include "starkware/air/trace.h"
#include "starkware/air/ziggy/ziggy_air.h"
#include "starkware/statement/statement.h"
#include "starkware/utils/json.h"

namespace starkware {

/*
  Public input:
    * Public key (array of 4 field elements).
    * Message (string).
  Private input:
    * Private key (bytearray of length 32).
*/
class ZiggyStatement : public Statement {
 public:
  using PublicKeyT = typename ZiggyAir::PublicKeyT;
  using PrivateKeyT = std::array<std::byte, Blake2s256::kDigestNumBytes>;
  using SecretPreimageT = typename ZiggyAir::SecretPreimageT;
  explicit ZiggyStatement(const JsonValue& public_input, std::optional<JsonValue> private_input);

  const Air& GetAir(bool is_zero_knowledge, size_t n_queries) override;

  /*
    Returns the serialization of: ["Ziggy", public key, message],
    where each of the field elements in public_key is written as 8 bytes.
  */
  const std::vector<std::byte> GetInitialHashChainSeed() const override;

  /*
    Returns the serialization of: ["Ziggy private seed", private key, message].
  */
  const std::vector<std::byte> GetZeroKnowledgeHashChainSeed() const override;

  Trace GetTrace(Prng* prng) const override;

  /*
    Evaluates the public input according to the private input, and returns a new public input JSON.

    This is mostly used when one wants to prove a statement, but doesn't yet have the public input.
    For this use case, one must set the --fix_public_input flag.
  */
  JsonValue FixPublicInput() override;

  std::string GetName() const override { return "ziggy"; }

 private:
  /*
    Returns the serialization of: ["Ziggy secret preimage seed", private key].
  */
  const std::vector<std::byte> GetSecretPreimageSeed() const;

  PublicKeyT public_key_;
  std::string message_;
  std::unique_ptr<ZiggyAir> air_;
  std::optional<PrivateKeyT> private_key_;
  std::optional<SecretPreimageT> secret_preimage_;
};

}  // namespace starkware

#endif  // STARKWARE_STATEMENT_ZIGGY_ZIGGY_STATEMENT_H_
