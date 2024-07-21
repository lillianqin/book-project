#pragma once

#include <cmath>
#include <concepts>
#include <functional>
#include <limits>

namespace bookproj {

// fixed precision price, Storage is the underlying integer storage type, Decimals
// is the scale to be applied.  The actual value is Storage / 10^Decimals
template <std::integral Storage, int Decimals> struct FPPrice {
  static_assert(Decimals >= 0 && Decimals <= 10);
  static constexpr double Scale = [](int d) {
    double scale = 1.0;
    for (int ii = 0; ii < d; ++ii, scale *= 10)
      ;
    return scale;
  }(Decimals);

  constexpr FPPrice() = default;
  constexpr FPPrice(double px) : value(round(px * Scale)) {}
  template <std::integral S, int D>
  constexpr explicit FPPrice(FPPrice<S, D> px)
      : value((D == Decimals) ? Storage(px.value) : round(px.value * (Scale / px.Scale))) {}

  constexpr explicit operator double() const { return value / Scale; }

  constexpr auto operator<=>(const FPPrice &) const = default;

  static constexpr FPPrice min() { return FPPrice(std::numeric_limits<Storage>::min()); }
  static constexpr FPPrice max() { return FPPrice(std::numeric_limits<Storage>::max()); }

  static constexpr FPPrice fromRaw(Storage px) { return FPPrice(px); }
  static constexpr Storage toRaw(FPPrice px) { return px.value; }

  struct Hash {
    constexpr size_t operator()(FPPrice p) const { return std::hash<Storage>()(p.value); }
  }; // namespace bookproj

private:
  static constexpr Storage round(double px) {
    if constexpr (sizeof(Storage) == sizeof(long long)) {
      return std::llround(px);
    } else {
      return std::lround(px);
    }
  }
  constexpr explicit FPPrice(Storage v) : value(v) {}
  Storage value = 0;
};
} // namespace bookproj

namespace std {
template <typename Storage, int Decimal> struct hash<bookproj::FPPrice<Storage, Decimal>> {
  size_t operator()(bookproj::FPPrice<Storage, Decimal> p) const {
    return typename bookproj::FPPrice<Storage, Decimal>::Hash{}(p);
  }
};
} // namespace std
