#ifndef STARKWARE_STL_UTILS_CONTAINERS_H_
#define STARKWARE_STL_UTILS_CONTAINERS_H_

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/error_handling/error_handling.h"

template <typename K, typename V>
std::set<K> Keys(const std::map<K, V>& m) {
  std::set<K> res;
  for (const auto& p : m) {
    res.insert(p.first);
  }
  return res;
}

template <typename T, typename V>
size_t Count(const T& container, const V& val) {
  return std::count(container.begin(), container.end(), val);
}

template <typename T>
typename T::value_type Sum(
    const T& container, const typename T::value_type& init = typename T::value_type(0)) {
  return std::accumulate(container.begin(), container.end(), init);
}

template <typename T>
bool AreDisjoint(const std::set<T>& set1, const std::set<T>& set2) {
  const std::set<T>& large = (set1.size() < set2.size()) ? set1 : set2;
  const std::set<T>& small = (set1.size() >= set2.size()) ? set1 : set2;
  for (const T& el : small) {
    if (large.count(el) > 0) {
      return false;
    }
  }
  return true;
}

template <typename T>
bool HasDuplicates(gsl::span<const T> values) {
  std::set<T> s;
  for (const T& x : values) {
    if (s.count(x) != 0) {
      return true;
    }
    s.insert(x);
  }
  return false;
}

/*
  Constructs an array of std::byte from integers.
  Usage:
    std::array<std::byte, 2> array = MakeByteArray<0x01, 0x02>();
*/
template <unsigned char... Bytes>
constexpr std::array<std::byte, sizeof...(Bytes)> MakeByteArray() {
  return {std::byte{Bytes}...};
}

template <typename C>
inline void WriteCommaSeparatedValues(const C& c, std::ostream* out) {
  for (auto it = c.begin(); it != c.end(); ++it) {
    if (it != c.begin()) {
      *out << ", ";
    }
    *out << *it;
  }
}

namespace std {  // NOLINT: modifying std is discouraged by clang-tidy.

template <typename T>
inline std::ostream& operator<<(std::ostream& out, const std::set<T>& s) {
  out << "{";
  WriteCommaSeparatedValues(s, &out);
  out << "}";
  return out;
}

template <typename T>
inline std::ostream& operator<<(std::ostream& out, const std::vector<T>& v) {
  out << "[";
  WriteCommaSeparatedValues(v, &out);
  out << "]";
  return out;
}

template <typename T>
inline std::ostream& operator<<(std::ostream& out, gsl::span<T> v) {
  out << "[";
  WriteCommaSeparatedValues(v, &out);
  out << "]";
  return out;
}

}  // namespace std

#endif  // STARKWARE_STL_UTILS_CONTAINERS_H_
