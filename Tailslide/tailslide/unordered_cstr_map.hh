#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include "cstring"

// based on Java string hashing algo, assumes null-terminated
template <class T = const char *>
struct CStrHash {
  constexpr std::size_t operator()(T x) const {
    size_t result = 0;
    const size_t prime = 31;
    for(size_t i=0; x[i]; ++i)
      result = x[i] + (result * prime);
    return result;
  }
};

template <class T = const char *>
struct CStrEqualTo {
  constexpr bool operator()(T x, T y) const {
    return strcmp(x, y) == 0;
  }
};

template<typename V>
using UnorderedCStrMap = std::unordered_multimap<
    const char *,
    V,
    CStrHash<const char *>,
    CStrEqualTo<const char *>
>;
