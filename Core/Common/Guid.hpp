#pragma once
#include <compare>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

struct Guid {
  std::uint64_t high = 0;
  std::uint64_t low = 0;
  constexpr bool is_nil() const noexcept { return high == 0 && low == 0; }
  constexpr explicit operator bool() const noexcept { return !is_nil(); }
  bool operator==(const Guid&) const = default;
  auto operator<=>(const Guid&) const = default;
};
Guid GenerateGuid();
std::string GuidToString(Guid);
Guid GuidFromString(std::string_view);
std::ostream& operator<<(std::ostream&, Guid);

template <typename Contains>
Guid GenerateUniqueGuid(Contains contains) {
  Guid id;
  do { id = GenerateGuid(); } while (!id || contains(id));
  return id;
}
