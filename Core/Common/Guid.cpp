#include "Guid.hpp"
#include <cerrno>
#include <ostream>
#include <stdexcept>
#include <system_error>
#include <sys/random.h>


Guid GenerateGuid() {
  unsigned char bytes[16]{};
  std::size_t filled = 0;
  while (filled < sizeof(bytes)) {
    auto count = getrandom(bytes + filled, sizeof(bytes) - filled, 0);
    if (count < 0) {
      if (errno == EINTR) continue;
      throw std::system_error(errno, std::generic_category(), "UUID randomness");
    }
    if (!count) throw std::runtime_error("Empty UUID random source");
    filled += static_cast<std::size_t>(count);
  }
  bytes[6] = (bytes[6] & 0x0f) | 0x40;
  bytes[8] = (bytes[8] & 0x3f) | 0x80;
  Guid id;
  for (std::size_t i = 0; i < 8; ++i) {
    id.high = (id.high << 8) | bytes[i];
    id.low = (id.low << 8) | bytes[i + 8];
  }
  return id;
}
std::string GuidToString(Guid id) {
  constexpr char hex[] = "0123456789abcdef";
  std::string result;
  result.reserve(36);
  for (unsigned i = 0; i < 32; ++i) {
    if (i == 8 || i == 12 || i == 16 || i == 20) result += '-';
    auto word = i < 16 ? id.high : id.low;
    result += hex[(word >> ((15 - i % 16) * 4)) & 15];
  }
  return result;
}
Guid GuidFromString(std::string_view text) {
  if (text.size() != 36) throw std::invalid_argument("Invalid GUID length");
  Guid id;
  unsigned digit = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (text[i] != '-') throw std::invalid_argument("Invalid GUID separator");
      continue;
    }
    unsigned value;
    auto c = text[i];
    if (c >= '0' && c <= '9') value = c - '0';
    else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
    else throw std::invalid_argument("Invalid GUID digit");
    auto& word = digit++ < 16 ? id.high : id.low;
    word = (word << 4) | value;
  }
  return id;
}
std::ostream& operator<<(std::ostream& out, Guid id) {
  return out << GuidToString(id);
}
