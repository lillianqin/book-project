#pragma once

#include "FPPrice.h"
#include <chrono>
#include <concepts>
#include <cstdint>
#include <string_view>

// A few types and concepts useful for book
namespace bookproj {
namespace orderbook {

enum class ReferenceNum : uint64_t {};
enum class Side : uint8_t { Bid, Ask };
static inline std::string_view sideName(Side side) { return side == Side::Bid ? "Bid" : "Ask"; }

using Price = FPPrice<int64_t, 8>;
using Quantity = int64_t;
using Timestamp = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

template <typename T>
concept IntegerLike =
    std::integral<T> || std::is_enum_v<T> ||
    (std::integral<typename T::underlying_type> && requires(T t) {
      static_cast<typename T::underlying_type>(t); // must be convertible to underlying type
    });

template <typename T>
concept FloatingLike = std::is_arithmetic_v<T> || requires(T t) { double(t); };

template <IntegerLike T> constexpr auto toUnderlying(T t) {
  if constexpr (std::integral<T>) {
    return t;
  } else if constexpr (std::is_enum_v<T>) {
    return static_cast<std::underlying_type_t<T>>(t);
  } else {
    return static_cast<typename T::underlying_type>(t);
  }
}

} // namespace orderbook
} // namespace bookproj