#include "OrderBookPrinter.h"
#include <format>

namespace bookproj {
namespace orderbook {

PrintParams inferPrintParams(const OrderBook::Half &half, int depth,
                             const PrintParams &minParams) {
  PrintParams params = minParams;
  static_assert(IntegerLike<size_t>);
  int nlevel = 0;
  for (auto iter = half.begin(); iter != half.end() && nlevel < depth; ++iter, ++nlevel) {
    auto level = iter->second;
    params.orderWidth = std::max(params.orderWidth, detail::integerWidth(level->size()));
    params.quantityWidth =
        std::max(params.quantityWidth, detail::integerWidth(level->totalShares));

    auto [nwidth, nfrac] = detail::floatingPointWidth(level->price);
    params.priceWidth = std::max(params.priceWidth, nwidth);
    params.pricePrecision = std::max(params.pricePrecision, nfrac);
  }
  return params;
}

PrintParams inferPrintParams(const OrderBook &order_book, CID cid, int depth,
                             const PrintParams &minParams) {
  PrintParams params = inferPrintParams(order_book.half(cid, Side::Bid), depth, minParams);
  return inferPrintParams(order_book.half(cid, Side::Ask), depth, params);
}

std::vector<std::string> printLevels(const OrderBook &order_book, CID cid, int depth) {
  PrintParams params = inferPrintParams(order_book, cid, depth, {});
  return printLevels(order_book, cid, depth, params);
}

// level view, iterate over levels on bid/ask
// each row has nth bid/ask levels in following format:
// (bid_orders) bid_quantity bid_price  ask_price ask_quantity (ask_orders)
std::vector<std::string> printLevels(const OrderBook &order_book, CID cid, int depth,
                                     const PrintParams &params) {
  auto &bids = order_book.half(cid, Side::Bid);
  auto &asks = order_book.half(cid, Side::Ask);

  auto fmtPrice([&](auto price) {
    if constexpr (IntegerLike<decltype(price)>) {
      return std::format("{}", price);
    } else {
      return std::format("{:.{}f}", double(price), params.pricePrecision);
    }
  });
  std::vector<std::string> lines;
  lines.reserve(depth);
  auto bidIter = bids.begin();
  auto askIter = asks.begin();
  for (int i = 0; i < depth; ++i) {
    std::string line;
    if (bidIter != bids.end()) {
      auto level = bidIter->second;
      line = std::format("({:>{}}) {:>{}} {:>{}}", level->numOrders(), params.orderWidth,
                         toUnderlying(level->totalShares), params.quantityWidth,
                         fmtPrice(level->price), params.priceWidth);
      ++bidIter;
    } else if (askIter != asks.end()) {
      line = std::string(params.orderWidth + params.quantityWidth + params.priceWidth + 4, ' ');
    }
    if (askIter != asks.end()) {
      auto level = askIter->second;
      line +=
          std::format("{:{}}{:<{}} {:<{}} ({:<{}})", ' ', params.bidAskSpaces,
                      fmtPrice(level->price), params.priceWidth, toUnderlying(level->totalShares),
                      params.quantityWidth, level->numOrders(), toUnderlying(params.orderWidth));
      ++askIter;
    }
    if (line.empty()) {
      break;
    }
    lines.push_back(std::move(line));
  }
  return lines;
}

std::string Order::toString() const {
  return std::format("refnum={} side={} size={} price={:.4f}", toUnderlying(refNum),
                     sideName(side), toUnderlying(quantity), double(price));
}

std::string ExecInfo::toString() const {
  return std::format("matchNum={} printable={}{}", matchNum, printable ? 'Y' : 'N',
                     hasPrice ? std::format(" price={}", double(price)) : "");
}

} // namespace orderbook
} // namespace bookproj
