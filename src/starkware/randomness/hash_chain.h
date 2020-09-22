#ifndef STARKWARE_RANDOMNESS_HASH_CHAIN_H_
#define STARKWARE_RANDOMNESS_HASH_CHAIN_H_

#include <cstddef>
#include <limits>
#include <vector>

#include "starkware/crypt_tools/blake2s_256.h"
#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

class HashChain {
 public:
  /*
    Initializes the hash chain to a value based on the public input and the constraints system.
    This ensures that initial randomness dependens on the current instance, and not on
    prover-defined data.
  */
  explicit HashChain(gsl::span<const std::byte> public_input_data);
  HashChain();

  void InitHashChain(gsl::span<const std::byte> bytes);

  void GetRandomBytes(gsl::span<std::byte> random_bytes_out);

  /*
    Hash data of arbitrary length into the hash chain.
  */
  void UpdateHashChain(gsl::span<const std::byte> raw_bytes);

  const Blake2s256& GetHashChainState() const { return hash_; }

  // In order to allow this class to be used by 'std::uniform_int_distribution' as a random bit
  // generator it must support the C++ standard UniformRandomBitGenerator prerequisites (see
  // https://en.cppreference.com/w/cpp/named_req/UniformRandomBitGenerator), that require the bit
  // generator to implement the 'operator()', 'min()', 'max()' methods and support result_type.
  // NOTE: This API is only used for random integer generation in tests and should not be used for
  // production as it limits the number of random bits that can be generated to 64.
  using result_type = uint64_t;
  static constexpr result_type min() { return 0; }                                     // NOLINT
  static constexpr result_type max() { return std::numeric_limits<uint64_t>::max(); }  // NOLINT
  result_type operator()() {
    result_type return_val;
    GetRandomBytes(gsl::make_span<result_type>(&return_val, 1).template as_span<std::byte>());
    return return_val;
  }

 private:
  /*
    A standard way to generate any number of pseudo random bytes from a given digest and
    hash function is to hash the digest with an incrementing counter until sufficient bytes are
    generated.
    This function takes a counter and a hash, and returns their combined hash.
  */
  static Blake2s256 HashWithCounter(const Blake2s256& hash, uint64_t counter);

  /*
    Adds additional random bytes by hashing the value of the given counter together with the
    current hash chain. num_bytes must be equal or less than the hash digest size.
  */
  void GetMoreRandomBytesUsingHashWithCounter(
      uint64_t counter, gsl::span<std::byte> random_bytes_out);

  Blake2s256 hash_;
  std::array<std::byte, Blake2s256::kDigestNumBytes * 2> spare_bytes_{};
  size_t num_spare_bytes_ = 0;
  size_t counter_ = 0;
};

}  // namespace starkware

#endif  // STARKWARE_RANDOMNESS_HASH_CHAIN_H_
