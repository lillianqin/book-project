#include "digest/sha256.h"
#include "itch50HistDataSource.h"
#include "itch50OrderBook.h"
#include "itch50RawParser.h"
#include "orderbook/OrderBook.h"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace bookproj;
using itch50::CIndex;
using itch50::StockLocateMap;
using itch50::Symbol;
using itch50::Timestamp;

using orderbook::BookID;
using orderbook::CID;
using orderbook::ExecInfo;
using orderbook::Order;
using orderbook::OrderBook;
using orderbook::Price;
using orderbook::Quantity;
using orderbook::Side;

struct Listener : public orderbook::BookListener {

  Listener(const OrderBook &book_, int depth_, Timestamp start_, Timestamp end_)
      : book(book_), startTime(start_), endTime(end_), depth(depth_) {}

  void onNewOrder(BookID book, const Order *order) override {
    if (inRange(order->updateTime)) {
      update(order->cid, order);
    }
  }

  void onDeleteOrder(BookID book, const Order *order, Quantity oldQuantity) override {
    if (inRange(order->updateTime)) {
      update(order->cid, order, oldQuantity);
    }
  }

  void onReplaceOrder(BookID book, const Order *order, const Order *oldOrder) override {
    if (inRange(order->updateTime)) {
      update(order->cid, order, oldOrder);
    }
  }

  void onExecOrder(BookID book, const Order *order, Quantity oldQuantity, Quantity fillQuantity,
                   const ExecInfo &ei) override {
    if (inRange(order->updateTime)) {
      update(order->cid, order, oldQuantity, fillQuantity, ei);
    }
  }

  void onUpdateOrder(BookID book, const Order *order, Quantity oldQuantity,
                     Price oldPrice) override {
    if (inRange(order->updateTime)) {
      update(order->cid, order, oldQuantity, oldPrice);
    }
  }

  size_t updates() const { return numUpdates; }
  std::string digestStr() { return digest.digest(); }

private:
  template <typename... Args> void update(CID cid, const Args &...args) {
    serialize(cid);
    (serialize(args), ...);
    serializeBook(cid);
    digest.update(buffer.data(), buffer.size());
    buffer.clear();
    ++numUpdates;
  }

  void serialize(const Order *order) {
    serialize(order->refNum);
    serialize(order->side != Side::Bid);
    serialize(order->quantity);
    serialize(order->price);
    serialize(order->updateTime);
  }

  void serialize(const OrderBook::Level *level) {
    serialize(level->numOrders());
    serialize(level->price);
    serialize(level->side() != Side::Bid);
    serialize(level->totalShares);
  }

  template <orderbook::IntegerLike T> void serialize(T t) {
    auto ul = orderbook::toUnderlying(t);
    size_t olen = buffer.size();
    buffer.resize(olen + sizeof(ul));
    memcpy(buffer.data() + olen, &ul, sizeof(ul));
  }

  void serialize(Price px) {
    double price = double(px);
    size_t olen = buffer.size();
    buffer.resize(olen + sizeof(price));
    memcpy(buffer.data() + olen, &price, sizeof(price));
  }

  void serialize(Timestamp ts) {
    uint64_t count = ts.time_since_epoch().count();
    serialize(count);
  }

  void serialize(const ExecInfo &ei) {
    serialize(ei.matchNum);
    serialize(ei.printable);
    if (ei.hasPrice) {
      serialize(ei.price);
    }
  }

  void serializeBook(CID cid) {
    // hash depth levels of the book
    for (int ii = 0; ii < depth; ++ii) {
      const auto &bid = book.nthLevel(cid, Side::Bid, ii);
      if (bid) {
        serialize(bid);
      }
      const auto &ask = book.nthLevel(cid, Side::Ask, ii);
      if (ask) {
        serialize(ask);
      }
    }
  }

  bool inRange(Timestamp ts) const { return ts >= startTime && ts <= endTime; }
  const OrderBook &book;
  const Timestamp startTime;
  const Timestamp endTime;
  const int depth;
  size_t numUpdates = 0;

  // buffer to store the pieces of bytes from one update, before sending to digest
  std::vector<std::byte> buffer;
  digest::SHA256 digest;
};

using QuoteHandler = itch50::Itch50QuoteHandler;
using SymbolHandler = itch50::Itch50SymbolHandler;

// return false if file is not found
std::pair<size_t, std::string> sha256sum(const std::vector<std::string> &symbols, int depth,
                                         int date) {
  OrderBook book(BookID{0});
  book.resize(CID(symbols.size()));

  // quote/misc handlers use StockLocateMap to filter symbols
  StockLocateMap stockLocateMap;
  CIndex cindex;
  for (const auto &symbol : symbols) {
    cindex.findOrInsert(Symbol(symbol));
  }

  using namespace std::chrono_literals;
  auto midnight = datasource::Itch50HistDataSource::midnightNYTime(date);
  Listener listener(book, depth, midnight, midnight + 23h + 59min + 59s);
  SymbolHandler symbolHandler(cindex, stockLocateMap, false);
  QuoteHandler quoteHandler(book, stockLocateMap, midnight, false);
  book.addListener(&listener);

  std::unique_ptr<datasource::Itch50HistDataSource> source;
  try {
    source.reset(new datasource::Itch50HistDataSource(20191230));
  } catch (const std::runtime_error &e) {
    std::cerr << "Error creating data source: " << e.what() << std::endl;
    return {0, ""};
  }

  while (source->hasMessage()) {
    auto result = itch50::parseMessage(source->nextMessage(), symbolHandler, quoteHandler);
    if (result != itch50::ParseResultType::Success) {
      std::cerr << "Error parsing message: " << bookproj::itch50::toString(result)
                << " file offset: " << source->currentOffset() << std::endl;
      break;
    }
    source->advance();
  }

  book.removeListener(&listener);
  return {listener.updates(), listener.digestStr()};
}

TEST_CASE("itch50book") {
  std::vector<std::string> symbols{"AAPL", "MSFT", "GOOGL"};

  datasource::Itch50HistDataSource::setRootPath("/opt/data");
  int depth = 5;
  auto [updates, digest] = sha256sum(symbols, depth, 20191230);
  auto expected = "7f3e9dff6ce62cd38b15e93b35aa2775c4aca3dc27eea1a268106defd40de045";
  CHECK(digest == expected);
  CHECK(updates == 3504243);
}