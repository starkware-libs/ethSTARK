#ifndef STARKWARE_RANDOMNESS_PRNG_H_
#define STARKWARE_RANDOMNESS_PRNG_H_

#include <cstddef>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/randomness/hash_chain.h"

namespace starkware {

/*
  Pseudo Random Number Generator class

  Note: This class is not thread safe.
*/
class Prng {
 public:
  /*
    The default constructor initializes seed using system time.
  */
  Prng();
  ~Prng() = default;

  Prng& operator=(const Prng&) noexcept = delete;

  Prng(Prng&& src) = default;
  Prng& operator=(Prng&& other) noexcept {
    hash_chain_ = other.hash_chain_;
    return *this;
  }

  Prng Clone() const { return {*this}; }

  explicit Prng(gsl::span<const std::byte> bytes) : hash_chain_(HashChain(bytes)) {}

  /*
    Returns a random integer in the closed interval [min, max].
  */
  template <typename T>
  T UniformInt(T min, T max) {
    static_assert(std::is_integral<T>::value, "Type is not integral.");
    ASSERT_RELEASE(min <= max, "Invalid interval.");
    std::uniform_int_distribution<T> d(min, max);
    return d(hash_chain_);
  }

  /*
    Returns a vector of random integers in the closed interval [min, max].
  */
  template <typename T>
  std::vector<T> UniformIntVector(T min, T max, size_t n_elements) {
    static_assert(std::is_integral<T>::value, "Type is not integral");
    ASSERT_RELEASE(min <= max, "Invalid interval.");
    std::uniform_int_distribution<T> d(min, max);
    std::vector<T> return_vec;
    return_vec.reserve(n_elements);
    for (size_t i = 0; i < n_elements; ++i) {
      return_vec.push_back(d(hash_chain_));
    }
    return return_vec;
  }

  /*
    Returns a vector of random field elements in the closed interval [min, max].
  */
  template <typename T>
  std::vector<T> RandomFieldElementVector(size_t n_elements) {
    std::vector<T> return_vec;
    return_vec.reserve(n_elements);
    for (size_t i = 0; i < n_elements; ++i) {
      return_vec.push_back(T::RandomElement(this));
    }
    return return_vec;
  }

  void GetRandomBytes(gsl::span<std::byte> random_bytes_out) {
    hash_chain_.GetRandomBytes(random_bytes_out);
  }

  std::vector<std::byte> RandomByteVector(size_t n_elements) {
    std::vector<std::byte> return_vec(n_elements);
    GetRandomBytes(return_vec);
    return return_vec;
  }

  /*
    Returns a random hash.
  */
  Blake2s256 RandomHash() {
    return Blake2s256::InitDigestTo(RandomByteVector(Blake2s256::kDigestNumBytes));
  }

  /*
    Updates hash chain seed with new bytes.
  */
  void MixSeedWithBytes(gsl::span<const std::byte> raw_bytes) {
    hash_chain_.UpdateHashChain(raw_bytes);
  }

  std::array<std::byte, Blake2s256::kDigestNumBytes> GetPrngState() const {
    return hash_chain_.GetHashChainState().GetDigest();
  }

 private:
  HashChain hash_chain_;

  // Private copy construct to prevent copy by mistake, resulting in correlated randomness.
  Prng(const Prng&) = default;
};

}  // namespace starkware

#endif  // STARKWARE_RANDOMNESS_PRNG_H_
