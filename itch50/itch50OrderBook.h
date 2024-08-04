#pragma once

#include "itch50.h"
#include "orderbook/CIndex.h"
#include "orderbook/OrderBook.h"
#include "orderbook/Symbol.h"
#include <chrono>
#include <utility>

namespace bookproj {
namespace itch50 {

// itch stock locate, starting from 1, 0 is reserved as invalid
struct StockLocate {
  using underlying_type = uint16_t;
  constexpr StockLocate(uint16_t val_) : val(val_) {}
  constexpr bool valid() const { return val != 0; }
  constexpr explicit operator uint16_t() const { return val; }
  static StockLocate invalid() { return StockLocate(0); }
  auto operator<=>(const StockLocate &rhs) const = default;
  template <typename H> friend H AbslHashValue(H h, const StockLocate &sl) {
    return H::combine(std::move(h), sl.val);
  }

private:
  uint16_t val;
};

// a bidirectional hashmap between StockLocate and CID
// this is similar to a CIndex, but does not sequentially allocate CIDs
struct StockLocateMap {
  using CID = orderbook::CID;

  StockLocateMap() : locate2CID{{StockLocate::invalid(), CID::invalid()}} {}

  bool insert(StockLocate locate, CID cid) {
    assert(cid.valid() && locate.valid());
    if (locate2CID.emplace(locate, cid).second) {
      auto ind = orderbook::toUnderlying(cid);
      if (std::cmp_greater_equal(ind, cid2Locate.size())) {
        cid2Locate.resize(ind + 1, StockLocate::invalid());
      }
      cid2Locate[ind] = locate;
      return true;
    }
    return false;
  }

  StockLocate operator[](CID cid) const {
    if (cid.valid() && std::cmp_less(orderbook::toUnderlying(cid), cid2Locate.size())) {
      return cid2Locate[orderbook::toUnderlying(cid)];
    }
    return StockLocate::invalid();
  }

  CID operator[](StockLocate locate) const {
    if (auto it = locate2CID.find(locate); it != locate2CID.end()) {
      return it->second;
    }
    return CID::invalid();
  }

  // minus 1 for invalid mapping
  size_t size() const { return locate2CID.size() - 1; }

  void reserve(size_t n) {
    locate2CID.reserve(n);
    cid2Locate.reserve(n);
  }

private:
  absl::flat_hash_map<StockLocate, orderbook::CID> locate2CID;
  std::vector<StockLocate> cid2Locate;
};

// UTC time
using Timestamp = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

using Symbol = orderbook::Symbol<8>;
using CIndex = orderbook::CIndex<orderbook::CID, Symbol>;

struct Itch50QuoteHandler {
  using Book = orderbook::OrderBook;
  using Order = Book::OrderExt;
  using RefNum = orderbook::ReferenceNum;
  using Size = orderbook::Quantity;
  using Price = orderbook::Price;
  using CID = orderbook::CID;
  using Side = orderbook::Side;

  Itch50QuoteHandler(Book &book_, const StockLocateMap &lindex_, Timestamp midnight_,
                     bool addAllSymbols_)
      : book(book_), lindex(lindex_), midnight(midnight_), addAllSymbols(addAllSymbols_) {}

  void process(const AddOrder &msg) {
    CID cid = lindex[StockLocate(+msg.header.stockLocate)];
    if (cid.valid()) {
      book.newOrder(RefNum(+msg.orderReferenceNumber), cid,
                    msg.buySellIndicator == 'B' ? Side::Bid : Side::Ask, Size(msg.shares),
                    Price(double(+msg.price)), getTimestamp(msg.header));
    }
  }

  void process(const AddOrderMPID &msg) {
    CID cid = lindex[StockLocate(+msg.header.stockLocate)];
    if (cid.valid()) {
      book.newOrder(RefNum(+msg.orderReferenceNumber), cid,
                    msg.buySellIndicator == 'B' ? Side::Bid : Side::Ask, Size(msg.shares),
                    Price(double(+msg.price)), getTimestamp(msg.header));
    }
  }

  void process(const OrderExecuted &msg) {
    if (addAllSymbols || lindex[StockLocate(+msg.header.stockLocate)].valid()) {
      orderbook::ExecInfo ei;
      ei.printable = true;
      ei.matchNum = +msg.matchNumber;
      book.executeOrder(RefNum(+msg.orderReferenceNumber), Size(msg.executedShares), ei,
                        getTimestamp(msg.header));
    }
  }

  void process(const OrderExecutedWithPrice &msg) {
    if (addAllSymbols || lindex[StockLocate(+msg.header.stockLocate)].valid()) {
      orderbook::ExecInfo ei;
      ei.matchNum = +msg.matchNumber;
      ei.price = Price(double(+msg.executionPrice));
      ei.hasPrice = true;
      ei.printable = msg.printable == 'Y';
      book.executeOrder(RefNum(+msg.orderReferenceNumber), Size(msg.executedShares), ei,
                        getTimestamp(msg.header));
    }
  }

  void process(const OrderCancel &msg) {
    if (addAllSymbols || lindex[StockLocate(+msg.header.stockLocate)].valid()) {
      book.reduceOrderBy(RefNum(+msg.orderReferenceNumber), Size(+msg.canceledShares),
                         getTimestamp(msg.header));
    }
  }

  void process(const OrderDelete &msg) {
    if (addAllSymbols || lindex[StockLocate(+msg.header.stockLocate)].valid()) {
      book.deleteOrder(RefNum(+msg.orderReferenceNumber), getTimestamp(msg.header));
    }
  }

  void process(const OrderReplace &msg) {
    if (addAllSymbols || lindex[StockLocate(+msg.header.stockLocate)].valid()) {
      Order *oldOrder = book.findOrder(RefNum(+msg.originalOrderReferenceNumber));
      if (oldOrder == nullptr) {
        // unable to add new order since no side/mpid information
        LOG(WARNING) << "Order with refNum " << msg.originalOrderReferenceNumber
                     << " not found in replaceOrder, ignored";
      } else {
        oldOrder->updateTime = getTimestamp(msg.header);
        book.replaceOrder(oldOrder, RefNum(+msg.newOrderReferenceNumber), Size(+msg.shares),
                          Price(double(+msg.price)), oldOrder->updateTime);
      }
    }
  }

  // catchall, does nothing for other messages
  template <typename Msg> void process(const Msg &) {}

  Timestamp getTimestamp(const CommonHeader &header) const {
    return midnight + std::chrono::nanoseconds(nanosSinceMidnight(header.timestamp));
  }

  Book &book;
  const StockLocateMap &lindex;
  const Timestamp midnight;
  const bool addAllSymbols;
};

struct Itch50SymbolHandler {
  Itch50SymbolHandler(CIndex &cindex_, StockLocateMap &lindex_, bool addAll_)
      : cindex(cindex_), lindex(lindex_), addAll(addAll_) {
    if (addAll) {
      lindex.reserve(16384);
      cindex.reserve(16384);
    }
  }

  template <typename Msg>
    requires requires(Msg msg) {
      { msg.stock } -> std::same_as<char(&)[8]>;
    }
  void process(const Msg &msg) {
    handleSymbol(msg.stock, StockLocate(+msg.header.stockLocate));
  }

  template <typename Msg> void process(const Msg &) {}

private:
  void handleSymbol(const char (&stock)[8], StockLocate locate) {
    if (addAll) {
      if (locate.valid() && !lindex[locate].valid()) {
        if (auto cid = cindex.findOrInsert(Symbol(stockName(stock))); cid.valid()) {
          lindex.insert(locate, cid);
        } else {
          LOG(WARNING) << "CIndex full, unable to add symbol " << stockName(stock);
        }
      }
    } else if (lindex.size() < cindex.size()) {
      if (auto cid = cindex[Symbol(stockName(stock))]; cid.valid()) {
        lindex.insert(locate, cid);
      }
    }
  }

  CIndex &cindex;
  StockLocateMap &lindex;
  const bool addAll;
};

} // namespace itch50
} // namespace bookproj
