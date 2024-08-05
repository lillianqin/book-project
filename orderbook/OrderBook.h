#pragma once

#include "CIndex.h"
#include "ObjectPool.h"
#include "OrderCommon.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include <boost/intrusive/list.hpp>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

using namespace boost::intrusive;

namespace bookproj {
namespace orderbook {

struct Order {
  // first argument must be ReferenceNum
  Order(ReferenceNum refNum, CID cid, Side side, Quantity quantity, Price price, Timestamp tm)
      : refNum(refNum), cid(cid), side(side), quantity(quantity), price(price), createTime(tm),
        updateTime(tm) {}

  // basic fields an order must have
  ReferenceNum refNum;
  CID cid;
  Side side;
  Quantity quantity;
  Price price;

  Timestamp createTime;
  Timestamp updateTime;

  std::string toString() const;
};

enum class BookID : int32_t {};

struct ExecInfo {
  uint64_t matchNum = 0;
  bool printable = true;
  bool hasPrice = false;
  // price is set only if hasPrice is true, not using std::optional to save space
  Price price = {};

  std::string toString() const;
};

struct BookListener {
  virtual void onNewOrder(BookID book, const Order *order) = 0;
  virtual void onDeleteOrder(BookID book, const Order *order, Quantity oldQuantity) = 0;
  virtual void onReplaceOrder(BookID book, const Order *order, const Order *oldOrder) = 0;
  virtual void onExecOrder(BookID book, const Order *order, Quantity oldQuantity,
                           Quantity fillQuantity, const ExecInfo &ei) = 0;
  virtual void onUpdateOrder(BookID book, const Order *order, Quantity oldQuantity,
                             Price oldPrice) = 0;
};

// OrderBook class is an aggregate of books of all CIDs.  It has a hashmap from reference numbers
// to order objects of all CIDs (CID is integer index starting from 0, a CID 1-1 corresponds to a
// symbol name).  For each CID, it maintains 2 half books, one for bid orders, one for ask orders.
// A half book contains a linked list of levels, sorted by aggressiveness of the prices.  The level
// with the most aggressive price (also called the best price) is at the front of the list.  For
// the bid side, the highest priced order is at the front. For the ask side, the lowest priced
// order is at the front.  Each level is a linked list of orders, sorted by the time they are added
// to the book. Orders are inserted, deleted, or modified (reduced, executed or replaced).

class OrderBook {
public:
  struct Level;
  // instrument order to include linked list iterator
  struct OrderExt : public Order {
    using Order::Order;

    // a back pointer to the level that has this order
    Level *level = nullptr;

    list_member_hook<link_mode<normal_link>> hook;

  private:
    friend class OrderBook;
  };

  using OrderList =
      list<OrderExt,
           member_hook<OrderExt, list_member_hook<link_mode<normal_link>>, &OrderExt::hook>,
           constant_time_size<true>>;

  OrderBook(BookID id_) : bkid(id_) {};
  OrderBook(const OrderBook &) = delete;
  OrderBook &operator=(const OrderBook &) = delete;
  ~OrderBook() { clear(false); };

  // note maxCid is exclusive.  If maxCid is 10, then the CIDs are 0-9
  void resize(CID maxCid);

  // reserve space for hashmaps to avoid resizing overhead at runtime
  void reserve(size_t cidSize, size_t orderMapSize, size_t levelMapSize) {
    books.reserve(cidSize);
    orders.reserve(orderMapSize);
    levels.reserve(levelMapSize);
    orderPool.reserve(orderMapSize);
    levelPool.reserve(levelMapSize);
  }

  // add a listener
  void addListener(BookListener *listener) { listeners.push_back(listener); }
  void removeListener(BookListener *listener) { std::erase(listeners, listener); }

  // return the book id
  BookID id() const { return bkid; }

  // return the number of active (non-zero quantity) orders in book
  size_t numOrders() const { return orderCount; }
  size_t numLevels() const { return levels.size(); }

  // some stats for future hashmap sizing
  size_t maxNumOrders() const { return maxOrderCount; }
  size_t maxNumLevels() const { return maxLevelCount; }

  // look up an order
  OrderExt *findOrder(ReferenceNum refNum);
  const OrderExt *findOrder(ReferenceNum refNum) const;

  // add order to book, notifies listeners via onNewOrder
  OrderExt *newOrder(ReferenceNum refNum, CID cid, Side side, Quantity quantity, Price price,
                     Timestamp tm);

  // reduce order size in place, notifies listeners via onUpdateOrder
  void reduceOrderBy(OrderExt *order, Quantity changeQuantity, Timestamp ut);
  void reduceOrderBy(ReferenceNum refNum, Quantity changeQuantity, Timestamp ut);

  void reduceOrderTo(OrderExt *order, Quantity newQuantity, Timestamp ut);
  void reduceOrderTo(ReferenceNum refNum, Quantity newQuantity, Timestamp ut);

  // replace order in book with new one, notifies listeners via onReplaceOrder, the new order
  // must share the same cid and side as the old order. Note this is almost equivalent to a
  // deleteOrder followed by a newOrder, except that listeners will be notified once via
  // onReplaceOrder, instead of separate onDelteOrder and onNewOrder
  OrderExt *replaceOrder(OrderExt *order, ReferenceNum newRefNum, Quantity newQuantity,
                         Price newPrice, Timestamp tm);
  OrderExt *replaceOrder(ReferenceNum oldRefNum, ReferenceNum newRefNum, Quantity newQuantity,
                         Price newPrice, Timestamp tm);

  // delete an order from book, notifies listeners via onDeleteOrder
  void deleteOrder(OrderExt *order, Timestamp ut);
  void deleteOrder(ReferenceNum refNum, Timestamp ut);

  // (partially) fill an order.  If the order is completely filled, it will be
  // deleted, notifies listeners via onExecOrder
  void executeOrder(OrderExt *order, Quantity quantity, const ExecInfo &ei, Timestamp ut);
  void executeOrder(ReferenceNum refNum, Quantity quantity, const ExecInfo &ei, Timestamp ut);

  // clear all orders of a CID, notifies listeners via onDeleteOrder.  Note orders may be deleted
  // in any order.
  void clearBook(CID cid) { clear(cid, true); }

  struct Half;

  // price level for CID/side/price
  struct Level : public OrderList {
    Level(Half *half_, Price price_) : price(price_), totalShares(0), half(half_) {
      half->insert(price, this);
    }

    Level(const Level &) = delete;
    Level &operator=(const Level &) = delete;
    ~Level();

    Side side() const { return half->side; }
    CID cid() const { return half->cid; }

    size_t numOrders() const { return OrderList::size(); }

    Price price;
    Quantity totalShares;

    Half *half;

    list_member_hook<link_mode<normal_link>> hook;

  private:
    friend class OrderBook;
  };

  using LevelList =
      list<Level, member_hook<Level, list_member_hook<link_mode<normal_link>>, &Level::hook>,
           constant_time_size<true>>;

  struct LevelCompare {
    Side side;
    LevelCompare(Side s) : side(s) {}
    bool operator()(Price left, Price right) const {
      if (side == Side::Ask) {
        return left < right;
      } else {
        return left > right;
      }
    }
  };

  using LevelMap = absl::btree_map<Price, Level *, LevelCompare>;

  struct LevelKey {
    CID cid;
    Side side;
    Price price;
    LevelKey(CID cid_, Side side_, Price price_) : cid(cid_), side(side_), price(price_) {};
    bool operator==(const LevelKey &lk) const = default;
    template <typename H> friend H AbslHashValue(H h, const LevelKey &lk) {
      return H::combine(std::move(h), lk.cid, lk.side, lk.price);
    }
  };

  // get the best level for cid/side, nullptr if empty
  const Level *topLevel(CID cid, Side side) const;

  // get n-th best level for cid/side, nullptr if not enough levels, n==0 returns the best level
  const Level *nthLevel(CID cid, Side side, size_t n) const;

  // get a level for cid/side at the given price, nullptr if no such level atm
  const Level *getLevel(CID cid, Side side, Price price) const;

  // one side of the book of a CID
  struct Half : public LevelList {
    Half(CID cid, Side side) : cid(cid), side(side), sortedLevels(LevelCompare(side)) {}
    Half(const Half &) = delete;
    Half(Half &&) = default;
    Half &operator=(const Half &) = delete;
    Half &operator=(Half &&) = default;

    CID cid;
    Side side;

    LevelMap sortedLevels;

    LevelList::iterator insert(Price price, Level *level) {
      auto [iter, inserted] = sortedLevels.emplace(price, level);
      iter++;
      if (iter == sortedLevels.end()) {
        LevelList::push_back(*level);
        return LevelList::s_iterator_to(*level);
      }
      return LevelList::insert(LevelList::s_iterator_to(*iter->second), *level);
    }
  };

  // get a half book, useful for walking its levels
  const Half &half(CID cid, Side side) const {
    assert(toUnderlying(cid) >= 0 && std::cmp_less(toUnderlying(cid), books.size()));
    return books[toUnderlying(cid)].halves[side != Side::Bid];
  }

  // clear the entire book, in the order of CID value, delete all orders
  void clear(bool callListeners) {
    for (size_t ii = 0; ii < books.size(); ++ii) {
      clear(static_cast<CID>(ii), callListeners);
    }
  }

  // return true if the book is in a consistent state: orders are in right price levels, quantities
  // are positive, levels have correct total quantities, are non-empty and ordered accordingly to
  // price priority, orderCount is correct
  // note this is very slow, so only use for testing or debugging
  bool validate(CID cid) const;
  bool validate() const;

private:
  // create an order object, add to orderMap, if order with same refNum exists, call deleteOrder
  // on it, which will notify listeners via onDeleteOrder
  OrderExt *createOrder(ReferenceNum refNum, CID cid, Side side, Quantity quantity, Price price,
                        Timestamp tm);
  // remove order from orderMap and delete the object
  void destroyOrder(OrderExt *order);

  // insert order into its level, create level if none exists, donot call listeners
  void linkOrder(OrderExt *order);
  // remove order from its level level, delete level if empty, donot call listeners, donot
  // delete order
  void unlinkOrder(OrderExt *order);

  Level *findOrCreateLevel(CID cid, Side side, Price price);
  void destroyLevel(Level *level);

  // clear book for one CID, delete its orders
  void clear(CID cid, bool callListeners);

  // get an string for logging and order
  static std::string getLevelString(const Level &level);
  static std::string getHalfString(const Half &half);

  struct PerCIDBook {
    PerCIDBook(CID cid) : halves{Half{cid, Side::Bid}, Half{cid, Side::Ask}} {}
    Half halves[2];
  };

  const BookID bkid;

  // indexed by cid
  std::vector<PerCIDBook> books;
  std::vector<BookListener *> listeners;

  // Orders linked in price levels, this is the actual active number of orders in a book.
  // It doesnot include orders that are unlinked but not yet erased from orders map yet, because
  // we need to keep the unlinked order object alive for callback to listeners
  size_t orderCount = 0;

  // maximum number of orders in the book, for stats
  size_t maxOrderCount = 0;
  size_t maxLevelCount = 0;

  // map of order objects
  absl::flat_hash_map<ReferenceNum, OrderExt *, absl::Hash<ReferenceNum>> orders;

  // map of level objects
  absl::flat_hash_map<LevelKey, Level *> levels;

  ObjectPool<OrderExt> orderPool;
  ObjectPool<Level> levelPool;
}; // namespace bookproj

inline OrderBook::Level::~Level() {
  half->erase(LevelList::s_iterator_to(*this));
  half->sortedLevels.erase(price);
}

inline OrderBook::OrderExt *OrderBook::findOrder(ReferenceNum refNum) {
  auto it = orders.find(refNum);
  return it == orders.end() ? nullptr : it->second;
}

inline const OrderBook::OrderExt *OrderBook::findOrder(ReferenceNum refNum) const {
  auto it = orders.find(refNum);
  return it == orders.end() ? nullptr : it->second;
}

inline void OrderBook::linkOrder(OrderExt *order) {
  Level *level = findOrCreateLevel(order->cid, order->side, order->price);
  level->push_back(*order);
  level->totalShares += order->quantity;
  order->level = level;
  if (++orderCount > maxOrderCount) {
    maxOrderCount = orderCount;
  }
}

inline void OrderBook::unlinkOrder(OrderExt *order) {
  Level *level = order->level;
  level->erase(OrderList::s_iterator_to(*order));
  level->totalShares -= order->quantity;
  if (level->empty()) {
    assert(level->totalShares == 0);
    destroyLevel(level);
    order->level = nullptr;
  }
  --orderCount;
}

inline OrderBook::OrderExt *OrderBook::createOrder(ReferenceNum refNum, CID cid, Side side,
                                                   Quantity quantity, Price price, Timestamp tm) {
  auto [iter, inserted] = orders.try_emplace(refNum, nullptr);
  if (!inserted) {
    LOG(WARNING) << "Order with refNum " << toUnderlying(refNum)
                 << " already exists, deleting old one and creating new one";
    OrderExt *order = iter->second;
    unlinkOrder(order);
    for (auto &listener : listeners) {
      listener->onDeleteOrder(id(), order, order->quantity);
    }
    order->~OrderExt();
    order = new (order) OrderExt(refNum, cid, side, quantity, price, tm);
  } else {
    iter->second = orderPool.create(refNum, cid, side, quantity, price, tm);
  }
  return iter->second;
}

inline void OrderBook::destroyOrder(OrderExt *order) {
  orders.erase(order->refNum);
  orderPool.destroy(order);
}

inline OrderBook::OrderExt *OrderBook::newOrder(ReferenceNum refNum, CID cid, Side side,
                                                Quantity quantity, Price price, Timestamp tm) {
  OrderExt *order = createOrder(refNum, cid, side, quantity, price, tm);
  assert(static_cast<size_t>(toUnderlying(order->cid)) < books.size());
  linkOrder(order);
  for (auto &listener : listeners) {
    listener->onNewOrder(id(), order);
  }
  return order;
}

inline void OrderBook::reduceOrderBy(OrderExt *order, Quantity changeQuantity, Timestamp ut) {
  Quantity oldQuantity = order->quantity;
  if (order->quantity <= changeQuantity) {
    unlinkOrder(order);
    if (order->quantity < changeQuantity) {
      LOG(WARNING) << "Order with refNum " << toUnderlying(order->refNum)
                   << " has less remaining quantity (" << order->quantity
                   << ") than reduceBy quantity (" << changeQuantity << ")";
    }
    // unlinkOrder updated level->totalShares
    order->quantity = 0;
  } else {
    order->quantity -= changeQuantity;
    order->level->totalShares -= changeQuantity;
  }

  order->updateTime = ut;
  for (auto &listener : listeners) {
    listener->onUpdateOrder(id(), order, oldQuantity, order->price);
  }

  if (order->quantity == 0) {
    destroyOrder(order);
  }
}

inline void OrderBook::reduceOrderBy(ReferenceNum refNum, Quantity changeQuantity, Timestamp ut) {
  if (auto order = findOrder(refNum)) {
    reduceOrderBy(order, changeQuantity, ut);
  } else {
    LOG(WARNING) << "Order with refNum " << toUnderlying(refNum) << " not found in reduceBy";
  }
}

inline void OrderBook::reduceOrderTo(OrderExt *order, Quantity newQuantity, Timestamp ut) {
  if (newQuantity == 0) {
    deleteOrder(order, ut);
    return;
  }

  Quantity oldQuantity = order->quantity;
  if (oldQuantity < newQuantity) {
    LOG(WARNING) << "Order with refNum " << toUnderlying(order->refNum)
                 << " has less remaining quantity (" << oldQuantity << ") than reduceTo quantity ("
                 << newQuantity << "), increasing to new quantity";
  }
  order->quantity = newQuantity;
  order->level->totalShares -= oldQuantity - order->quantity;
  order->updateTime = ut;
  for (auto &listener : listeners) {
    listener->onUpdateOrder(id(), order, oldQuantity, order->price);
  }
}

inline void OrderBook::reduceOrderTo(ReferenceNum refNum, Quantity newQuantity, Timestamp ut) {
  if (auto order = findOrder(refNum)) {
    reduceOrderTo(order, newQuantity, ut);
  } else {
    LOG(WARNING) << "Order with refNum " << toUnderlying(refNum) << " not found in reduceTo";
  }
}

inline OrderBook::OrderExt *OrderBook::replaceOrder(OrderExt *order, ReferenceNum newRefNum,
                                                    Quantity newQuantity, Price newPrice,
                                                    Timestamp tm) {
  unlinkOrder(order);
  OrderExt *oldOrder = order;
  OrderExt *newOrder = nullptr;
  std::byte buf[sizeof(OrderExt)];

  // same reference number
  if (order->refNum == newRefNum) {
    // move old order to a temporary place, construct new order in its original place
    oldOrder = new (buf) OrderExt(std::move(*order));
    order->~OrderExt();
    newOrder =
        new (order) OrderExt(newRefNum, oldOrder->cid, oldOrder->side, newQuantity, newPrice, tm);
  } else {
    newOrder = createOrder(newRefNum, order->cid, order->side, newQuantity, newPrice, tm);
  }

  linkOrder(newOrder);
  for (auto &listener : listeners) {
    listener->onReplaceOrder(id(), oldOrder, newOrder);
  }

  if (order == oldOrder) {
    // free memory
    destroyOrder(oldOrder);
  } else {
    // storage is buf, just pseudo destruct
    oldOrder->~OrderExt();
  }

  return newOrder;
}

inline OrderBook::OrderExt *OrderBook::replaceOrder(ReferenceNum oldRefNum, ReferenceNum newRefNum,
                                                    Quantity newQuantity, Price newPrice,
                                                    Timestamp tm) {
  // have to define this inline due to llvm bug
  if (OrderExt *oldOrder = findOrder(oldRefNum)) {
    return replaceOrder(oldOrder, newRefNum, newQuantity, newPrice, tm);
  } else {
    LOG(WARNING) << "Order with refNum " << toUnderlying(oldRefNum)
                 << " not found in replaceOrder";
    return nullptr;
  }
}

inline void OrderBook::deleteOrder(OrderExt *order, Timestamp ut) {
  unlinkOrder(order);
  order->updateTime = ut;
  for (auto &listener : listeners) {
    listener->onDeleteOrder(id(), order, order->quantity);
  }
  destroyOrder(order);
}

inline void OrderBook::deleteOrder(ReferenceNum refNum, Timestamp ut) {
  // have to define this inline due to llvm bug
  if (auto order = findOrder(refNum)) {
    deleteOrder(order, ut);
  } else {
    LOG(WARNING) << "Order with refNum " << toUnderlying(refNum) << " not found in deleteOrder";
  }
}

inline void OrderBook::executeOrder(OrderExt *order, Quantity quantity, const ExecInfo &ei,
                                    Timestamp ut) {
  Level *level = order->level;
  Quantity oldQuantity = order->quantity;
  if (order->quantity <= quantity) {
    unlinkOrder(order);
    if (order->quantity < quantity) {
      LOG(WARNING) << "Order with refNum " << toUnderlying(order->refNum)
                   << " has less remaining quantity (" << order->quantity
                   << ") than execute quantity (" << quantity << ")";
    }
    // unlinkOrder updated level->totalShares
    // despite a shortfall, listeners still gets the reported quantity
    order->quantity = 0;
  } else {
    level->totalShares -= quantity;
    order->quantity -= quantity;
  }
  order->updateTime = ut;

  // decrement level totalShares
  for (auto &listener : listeners) {
    listener->onExecOrder(id(), order, oldQuantity, quantity, ei);
  }

  if (order->quantity == 0) {
    destroyOrder(order);
  }
}

inline void OrderBook::executeOrder(ReferenceNum refNum, Quantity quantity, const ExecInfo &ei,
                                    Timestamp ut) {
  // have to define this inline due to llvm bug
  if (auto order = findOrder(refNum)) {
    executeOrder(order, quantity, ei, ut);
  } else {
    LOG(WARNING) << "Order with refNum " << toUnderlying(refNum) << " not found in executeOrder";
  }
}

inline OrderBook::Level *OrderBook::findOrCreateLevel(CID cid, Side side, Price price) {
  auto &book = books[toUnderlying(cid)];
  auto &half = book.halves[side != Side::Bid];

  auto [iter, inserted] = levels.try_emplace(LevelKey(cid, side, price), nullptr);
  if (inserted) {
    iter->second = levelPool.create(&half, price);
    if (levels.size() > maxLevelCount) {
      maxLevelCount = levels.size();
    }
  }
  return iter->second;
}

inline void OrderBook::destroyLevel(Level *level) {
  levels.erase(LevelKey(level->cid(), level->side(), level->price));
  levelPool.destroy(level);
}

inline void OrderBook::clear(CID cid, bool callListeners) {
  assert(toUnderlying(cid) >= 0 && std::cmp_less(toUnderlying(cid), books.size()));
  auto &book = books[toUnderlying(cid)];
  for (auto &half : book.halves) {
    for (size_t numLevels = half.size(); numLevels; --numLevels) {
      Level *level = &half.front();
      for (size_t numOrders = level->size(); numOrders; --numOrders) {
        OrderExt *order = &level->front();
        unlinkOrder(order);
        if (callListeners) {
          for (auto &listener : listeners) {
            listener->onDeleteOrder(id(), order, order->quantity);
          }
        }
        destroyOrder(order);
      }
      // level should have been destroyed by last eraseOrder
    }
  }
}

inline void OrderBook::resize(CID maxCID) {
  auto ubound = toUnderlying(maxCID);
  assert(ubound > 0);
  if (std::cmp_less(ubound, books.size())) {
    for (auto cid = ubound; std::cmp_less(cid, books.size()); ++cid) {
      clear(static_cast<CID>(cid), false);
    }
    books.erase(books.begin() + ubound, books.end());
  } else {
    while (std::cmp_less(books.size(), ubound)) {
      books.emplace_back(static_cast<CID>(books.size()));
    }
  }
}

inline const OrderBook::Level *OrderBook::topLevel(CID cid, Side side) const {
  assert(toUnderlying(cid) >= 0 && std::cmp_less(toUnderlying(cid), books.size()));
  const auto &book = books[toUnderlying(cid)];
  const auto &half = book.halves[side != Side::Bid];
  return half.empty() ? nullptr : &half.front();
}

// get n-th best level for cid/side, nullptr if not enough levels
inline const OrderBook::Level *OrderBook::nthLevel(CID cid, Side side, size_t n) const {
  assert(toUnderlying(cid) >= 0 && std::cmp_less(toUnderlying(cid), books.size()));
  const auto &book = books[toUnderlying(cid)];
  const auto &half = book.halves[side != Side::Bid];
  if (n >= half.size()) {
    return nullptr;
  }
  auto iter = half.begin();
  std::advance(iter, n);
  return &(*iter);
}

inline const OrderBook::Level *OrderBook::getLevel(CID cid, Side side, Price price) const {
  auto iter = levels.find(LevelKey(cid, side, price));
  return iter == levels.end() ? nullptr : iter->second;
}

inline std::string OrderBook::getLevelString(const Level &level) {
  return std::format("CID={} Side={} Price={} TotalShares={}", toUnderlying(level.cid()),
                     sideName(level.side()), static_cast<double>(level.price), level.totalShares);
}

inline std::string OrderBook::getHalfString(const Half &half) {
  return std::format("CID={} Side={} Depth={}", toUnderlying(half.cid), sideName(half.side),
                     half.size());
}

inline bool OrderBook::validate(CID cid) const {
  bool success = true;
  assert(toUnderlying(cid) >= 0 && std::cmp_less(toUnderlying(cid), books.size()));
  const auto &book = books[toUnderlying(cid)];
  const auto &bidHalf = book.halves[0];
  if (!std::is_sorted(bidHalf.begin(), bidHalf.end(),
                      [](auto &a, auto &b) { return a.price > b.price; })) {
    LOG(ERROR) << "Bid levels are not ordered by price for half: " << getHalfString(bidHalf);
    success = false;
  }
  const auto &askHalf = book.halves[1];
  if (!std::is_sorted(askHalf.begin(), askHalf.end(),
                      [](auto &a, auto &b) { return a.price < b.price; })) {
    LOG(ERROR) << "Ask levels are not ordered by price for half: " << getHalfString(askHalf);
    success = false;
  }

  for (auto &half : book.halves) {
    for (auto &levelref : half) {
      const Level *level = &levelref;
      if (level->half != &half) {
        LOG(ERROR) << "Level half mismatch, level: " << getLevelString(*level)
                   << ", half: " << getHalfString(half);
        success = false;
      }
      if (level->empty()) {
        LOG(ERROR) << "Level is empty, " << getLevelString(*level);
        success = false;
      }

      Quantity totalShares = 0;
      for (const auto &oref : *level) {
        auto order = &oref;
        if (order->level != level || order->cid != cid || order->side != level->side() ||
            order->price != level->price) {
          LOG(ERROR) << "Order level mismatch, order: " << toUnderlying(cid) << ", "
                     << order->toString() << ", level: " << getLevelString(*level);
          success = false;
        }
        if (order->quantity <= 0) {
          LOG(ERROR) << "Order quantity is non-positive, order: " << order->toString();
          success = false;
        }
        if (auto oit = orders.find(order->refNum); oit == orders.end()) {
          LOG(ERROR) << "Order not found in orders map, " << order->toString();
          success = false;
        } else if (oit->second != order) {
          LOG(ERROR) << "Order is not the same as in orders map, order: " << order->toString()
                     << ", in order map: " << oit->second->toString();
          success = false;
        }
        totalShares += order->quantity;
      }
      if (level->totalShares != totalShares) {
        LOG(ERROR) << "Level totalShares is mismatched, LevelTotalShares=" << level->totalShares
                   << " SumOfOrderQuantities=" << totalShares
                   << " level: " << getLevelString(*level);
        success = false;
      }
    }
  }
  return success;
}

inline bool OrderBook::validate() const {
  bool success = true;
  for (size_t ii = 0; ii < books.size(); ++ii) {
    success &= validate(static_cast<CID>(ii));
  }
  // all orders are linked at this point
  if (orderCount != orders.size()) {
    LOG(ERROR) << "Order count mismatch, OrderCount=" << orderCount
               << " OrdersMapSize=" << orders.size();
    success = false;
  }
  for (auto &order : orders) {
    if (order.second->level == nullptr) {
      LOG(ERROR) << "Order is not linked, " << order.second->toString();
      success = false;
    }
  }
  for (auto &level : levels) {
    if (level.second->empty()) {
      LOG(ERROR) << "Level is not linked, " << getLevelString(*level.second);
      success = false;
    }
  }
  size_t totalLevels = 0;
  size_t totalOrders = 0;
  for (auto &book : books) {
    for (auto &half : book.halves) {
      totalLevels += half.size();
      for (auto &level : half) {
        totalOrders += level.size();
      }
    }
  }
  if (totalLevels != levels.size()) {
    LOG(ERROR) << "Level count mismatch, LevelCount=" << totalLevels
               << " LevelsMapSize=" << levels.size();
    success = false;
  }
  if (totalOrders != orders.size() || totalOrders != orderCount) {
    LOG(ERROR) << "Order count mismatch, CountedOrders=" << totalOrders
               << " OrderCount=" << orderCount << " OrdersMapSize=" << orders.size();
    success = false;
  }
  return success;
}

} // namespace orderbook
} // namespace bookproj
