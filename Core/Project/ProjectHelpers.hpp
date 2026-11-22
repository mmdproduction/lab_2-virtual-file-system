#pragma once
#include "Sequence/ArraySequence/ArraySequence.hpp"
template <class T>
bool Contains(const ArraySequence<T>& a, const T& v) {
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a.get(i) == v) return true;
  return false;
}
