#include "starkware/commitment_scheme/packer_hasher.h"

#include <algorithm>

#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {

namespace {

/*
  Computes the number of elements that go in each package. Designed so that each package contains
  the minimal number of elements possible, without introducing trivial efficiency issues. The
  result will be at most max_n_elements.
*/
size_t ComputeNumElementsInPackage(
    const size_t size_of_element, const size_t size_of_package, const uint64_t max_n_elements) {
  ASSERT_RELEASE(size_of_element > 0, "An element must be at least of length 1 byte.");
  if (size_of_element >= size_of_package) {
    return 1;
  }
  const size_t elements_fit_in_package = (size_of_package - 1) / size_of_element + 1;
  return static_cast<size_t>(std::min(Pow2(Log2Ceil(elements_fit_in_package)), max_n_elements));
}

/*
  Partitions the sequence to n_elements equal sub-sequences, hashing
  each separately, and returning the resulting sequence of hashes as vector of bytes.
*/
std::vector<std::byte> HashElements(gsl::span<const std::byte> data, size_t n_elements) {
  if (n_elements == 0 && data.empty()) {
    return {};
  }
  const size_t element_size = SafeDiv(data.size(), n_elements);
  std::vector<std::byte> res;
  res.reserve(n_elements * Blake2s256::kDigestNumBytes);
  size_t pos = 0;
  for (size_t i = 0; i < n_elements; ++i, pos += element_size) {
    const auto hash_as_bytes_array =
        (Blake2s256::HashBytesWithLength(data.subspan(pos, element_size))).GetDigest();
    res.insert(res.end(), hash_as_bytes_array.begin(), hash_as_bytes_array.end());
  }
  return res;
}

}  // namespace

// Implementation of PackerHasher.

PackerHasher::PackerHasher(size_t size_of_element, size_t n_elements)
    : k_n_elements_in_package(ComputeNumElementsInPackage(
          size_of_element, 2 * Blake2s256::kDigestNumBytes, n_elements)),
      k_n_packages(SafeDiv(n_elements, k_n_elements_in_package)),
      k_size_of_element_(size_of_element) {
  ASSERT_RELEASE(
      IsPowerOfTwo(n_elements), "Can only handle total number of elements that is a power of 2.");
  ASSERT_RELEASE(
      IsPowerOfTwo(k_n_elements_in_package),
      "Can only pack number of elements that is a power of 2.");
  // The following may indicate an error in parameters.
  ASSERT_RELEASE(
      n_elements >= k_n_elements_in_package,
      "There are less elements overall than there should be in a single package.");
}

std::vector<std::byte> PackerHasher::PackAndHash(gsl::span<const std::byte> data) const {
  if (data.empty()) {
    return {};
  }
  size_t n_elements_in_data = SafeDiv(data.size(), k_size_of_element_);
  size_t n_packages = SafeDiv(n_elements_in_data, k_n_elements_in_package);
  return HashElements(data, n_packages);
}

std::vector<uint64_t> PackerHasher::GetElementsInPackages(
    gsl::span<const uint64_t> packages) const {
  // Finds all elements that belong to the required packages.
  std::vector<uint64_t> elements_needed;
  elements_needed.reserve(packages.size() * k_n_elements_in_package);
  for (uint64_t package : packages) {
    for (uint64_t i = package * k_n_elements_in_package;
         i < (package + 1) * k_n_elements_in_package; ++i) {
      elements_needed.push_back(i);
    }
  }
  return elements_needed;
}

std::vector<uint64_t> PackerHasher::ElementsRequiredToComputeHashes(
    const std::set<uint64_t>& elements_known) const {
  std::set<uint64_t> packages;

  // Get package indices of known_elements.
  for (const uint64_t el : elements_known) {
    const uint64_t package_id = el / k_n_elements_in_package;
    ASSERT_RELEASE(package_id < k_n_packages, "Query out of range.");
    packages.insert(package_id);
  }
  // Return only elements that belong to packages but are not known.
  const auto all_packages_elements =
      GetElementsInPackages(std::vector<uint64_t>{packages.begin(), packages.end()});
  std::vector<uint64_t> required_elements;
  std::set_difference(
      all_packages_elements.begin(), all_packages_elements.end(), elements_known.begin(),
      elements_known.end(), std::inserter(required_elements, required_elements.begin()));
  return required_elements;
}

std::map<uint64_t, std::vector<std::byte>> PackerHasher::PackAndHash(
    const std::map<uint64_t, std::vector<std::byte>>& elements) const {
  std::set<uint64_t> packages;
  std::map<uint64_t, std::vector<std::byte>> hashed_packages;
  // Deduce required packages.
  for (const auto& key_val : elements) {
    packages.insert(key_val.first / k_n_elements_in_package);
  }

  std::vector<std::byte> packed_elements(k_size_of_element_ * k_n_elements_in_package);
  // Pack elements together and hash them. Stores each hash as a vector of bytes.
  for (const uint64_t package : packages) {
    size_t pos_in_packed_elements = 0;
    for (uint64_t i = package * k_n_elements_in_package;
         i < (package + 1) * k_n_elements_in_package; ++i) {
      const std::vector<std::byte>& element_data = elements.at(i);
      ASSERT_RELEASE(
          element_data.size() == k_size_of_element_, "Element size mismatches the one declared.");
      std::copy(
          element_data.begin(), element_data.end(),
          packed_elements.begin() + pos_in_packed_elements);
      pos_in_packed_elements += element_data.size();
    }
    // Hashes a package of elements and stores it as a vector of bytes.
    const auto bytes_array = Blake2s256::HashBytesWithLength(packed_elements).GetDigest();
    hashed_packages[package] = {bytes_array.begin(), bytes_array.end()};
  }
  return hashed_packages;
}

}  // namespace starkware
