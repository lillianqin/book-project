#include "OrderBook.h"

namespace bookproj::orderbook {
std::string OrderBook::getLevelString(const Level &level) {
  return std::format("CID={} Side={} Price={} TotalShares={}", toUnderlying(level.cid()),
                     sideName(level.side()), static_cast<double>(level.price), level.totalShares);
}

std::string OrderBook::getHalfString(const Half &half) {
  return std::format("CID={} Side={} Depth={}", toUnderlying(half.cid), sideName(half.side),
                     half.size());
}

bool OrderBook::validate(CID cid) const {
  bool success = true;
  assert(toUnderlying(cid) >= 0 && std::cmp_less(toUnderlying(cid), books.size()));
  const auto &book = books[toUnderlying(cid)];
  const auto &bidHalf = book.halves[0];
  if (!std::is_sorted(bidHalf.begin(), bidHalf.end(),
                      [](auto &a, auto &b) { return a.first > b.first; })) {
    LOG(ERROR) << "Bid levels are not ordered by price for half: " << getHalfString(bidHalf);
    success = false;
  }
  const auto &askHalf = book.halves[1];
  if (!std::is_sorted(askHalf.begin(), askHalf.end(),
                      [](auto &a, auto &b) { return a.first < b.first; })) {
    LOG(ERROR) << "Ask levels are not ordered by price for half: " << getHalfString(askHalf);
    success = false;
  }

  for (auto &half : book.halves) {
    for (auto &levelref : half) {
      const Level *level = levelref.second;
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

bool OrderBook::validate() const {
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
        totalOrders += level.second->size();
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
} // namespace bookproj::orderbook