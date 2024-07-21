#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string_view>

namespace bookproj {
namespace orderbook {

// Symbol is the name of a stock, it is templatized with max length of the name. In the US, a stock
// name is 8 characters at most.
template <size_t Len> struct Symbol {
  static constexpr Symbol invalid() { return Symbol("<INVALD>"); }

  constexpr Symbol(std::string_view sym) : value(makeValue(sym)) {}

  constexpr bool operator==(Symbol s) const { return value == s.value; }
  constexpr bool operator!=(Symbol s) const { return value != s.value; }

  constexpr std::string_view view() const {
    // find the first '\0' in the value, without std::find to avoid <algorithm> header
    auto it = std::cbegin(value);
    for (auto end = std::cend(value); it != end && *it != '\0'; ++it)
      ;
    return std::string_view(std::begin(value), it);
  }

  constexpr bool valid() const { return *this != invalid(); }

  struct Hash {
    constexpr size_t operator()(Symbol s) const {
      if constexpr (Len == 8) {
        return std::bit_cast<uint64_t>(s.value);
      } else if constexpr (Len == 4) {
        return std::bit_cast<uint32_t>(s.value);
      } else {
        return std::hash<std::string_view>{}(s.view());
      }
    }
  };

private:
  // not 0-terminated
  using ValueType = std::array<char, Len>;
  static constexpr ValueType makeValue(std::string_view sym) {
    ValueType result = {};
    for (size_t i = 0; i < result.size() && i < sym.size(); ++i) {
      result[i] = sym[i];
    }
    return result;
  }
  ValueType value = {};
};

} // namespace orderbook
} // namespace bookproj

namespace std {
template <size_t Len> struct hash<bookproj::orderbook::Symbol<Len>> {
  using BaseType = bookproj::orderbook::Symbol<Len>;
  using is_avalanching = void; // for ankerl::unordered_dense::map
  constexpr size_t operator()(const BaseType &s) const { return typename BaseType::Hash{}(s); }
};
} // namespace std
