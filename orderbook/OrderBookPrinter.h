#pragma once

#include "OrderBook.h"
#include <cmath>
#include <vector>

namespace bookproj {
namespace orderbook {

struct PrintParams {
  size_t orderWidth = 0;
  size_t quantityWidth = 0;
  size_t priceWidth = 0;
  size_t pricePrecision = 0;
  size_t bidAskSpaces = 3;
};

namespace detail {

template <IntegerLike T> constexpr size_t integerWidth(T t) {
  char buf[128];
  return std::to_chars(buf, buf + sizeof(buf), toUnderlying(t)).ptr - buf;
}

template <FloatingLike T> std::pair<size_t, size_t> floatingPointWidth(T t) {
  // TODO: make this constexpr when compiler supports constexpr cmath per C++23
  constexpr double epsilon = 1e-10;
  const double v = double(t);
  double absv = std::abs(v);
  double residual = std::abs(absv - std::round(absv)) > epsilon ? absv - std::floor(absv) : 0.0;
  // digits to the left of decimal point
  size_t nleft = 1;
  while (absv >= 10.0) {
    absv /= 10;
    nleft++;
  }

  // digits to the right of decimal point, IEEE-754 double has 53-bits
  // mantissa (including impicit leading bit), at most 16 decimal places
  size_t nfrac = 0;
  double scale = 10.0;
  double remaining = residual;
  while ((nfrac + nleft) < 17 && std::abs(remaining) > epsilon) {
    remaining = residual - std::round(residual * scale) / scale;
    scale *= 10;
    nfrac++;
  }
  // in case negative, add one more for the sign
  size_t nwidth = nleft + nfrac + (nfrac > 0) + (v < 0);
  return {nwidth, nfrac};
}

} // namespace detail

PrintParams inferPrintParams(const OrderBook::Half &half, int depth, const PrintParams &minParams);

PrintParams inferPrintParams(const OrderBook &order_book, CID cid, int depth,
                             const PrintParams &minParams);

std::vector<std::string> printLevels(const OrderBook &order_book, CID cid, int depth);

// level view, iterate over levels on bid/ask
// each row has nth bid/ask levels in following format:
// (bid_orders) bid_quantity bid_price  ask_price ask_quantity (ask_orders)
std::vector<std::string> printLevels(const OrderBook &order_book, CID cid, int depth,
                                     const PrintParams &params);

} // namespace orderbook
} // namespace bookproj
