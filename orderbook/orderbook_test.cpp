#include "OrderBook.h"
#include <algorithm>
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"

int main(int argc, char *argv[]) {
  int result = Catch::Session().run(argc, argv);
  return result;
}

using namespace bookproj::orderbook;

struct Listener : public BookListener {

  std::vector<std::tuple<BookID, const Order *>> newOrders;
  std::vector<std::tuple<BookID, const Order *, Quantity>> deleteOrders;
  std::vector<std::tuple<BookID, const Order *, const Order *>> replaceOrders;
  std::vector<std::tuple<BookID, const Order *, Quantity, Quantity, Price>> execOrders;
  std::vector<std::tuple<BookID, const Order *, Quantity, Price>> updateOrders;

  void onNewOrder(BookID book, const Order *order) override {
    newOrders.emplace_back(book, order);
  }

  void onDeleteOrder(BookID book, const Order *order, Quantity oldQuantity) override {
    deleteOrders.emplace_back(book, order, oldQuantity);
  }

  void onReplaceOrder(BookID book, const Order *order, const Order *oldOrder) override {
    replaceOrders.emplace_back(book, order, oldOrder);
  }

  void onExecOrder(BookID book, const Order *order, Quantity oldQuantity, Quantity fillQuantity,
                   const ExecInfo &ei) override {
    execOrders.emplace_back(book, order, oldQuantity, fillQuantity, ei.price);
  }

  void onUpdateOrder(BookID book, const Order *order, Quantity oldQuantity,
                     Price oldPrice) override {
    updateOrders.emplace_back(book, order, oldQuantity, oldPrice);
  }
};

static_assert(sizeof(absl::btree_map<Price, OrderBook::Level *>) == 24);
static_assert(sizeof(absl::btree_set<OrderBook::Level *>) == 24);

TEST_CASE("Basic") {
  OrderBook book(BookID(0));
  CHECK(book.id() == BookID(0));
  CHECK(book.numOrders() == 0);

  book.clear(false);
  book.reserve(20, 20, 20);
  book.resize(CID(10));
  book.newOrder(ReferenceNum(1), CID(0), Side::Bid, /*Quantity*/ 100, 100.0, Timestamp{});
  CHECK(book.numOrders() == 1);
  auto order1 = book.findOrder(ReferenceNum(1));
  REQUIRE(order1 != nullptr);
  auto level1 = book.topLevel(CID(0), Side::Bid);
  REQUIRE(level1 != nullptr);
  CHECK(level1->price == 100.0);
  CHECK(level1->totalShares == 100);
  CHECK(level1->numOrders() == 1);
  CHECK(&level1->front() == order1);
  CHECK(order1->refNum == ReferenceNum(1));
  CHECK(order1->quantity == 100);
  CHECK(order1->price == 100.0);
  CHECK(order1->cid == CID(0));
  CHECK(order1->side == Side::Bid);
  CHECK(order1->level == level1);
  CHECK(book.nthLevel(CID(0), Side::Bid, 0) == level1);
  CHECK(book.nthLevel(CID(0), Side::Bid, 1) == nullptr);
  CHECK(book.getLevel(CID(0), Side::Bid, 100.0) == level1);
  CHECK(book.getLevel(CID(0), Side::Bid, 100.1) == nullptr);
  CHECK(book.getLevel(CID(0), Side::Ask, 100.0) == nullptr);

  book.newOrder(ReferenceNum(2), CID(1), Side::Ask, /*Quantity */ 100,
                /*Price*/ 102.00, Timestamp{});
  CHECK(book.numOrders() == 2);
  auto order2 = book.findOrder(ReferenceNum(2));
  REQUIRE(order2 != nullptr);
  auto level2 = book.topLevel(CID(1), Side::Ask);
  REQUIRE(level2 != nullptr);
  CHECK(book.findOrder(ReferenceNum(1)) == order1);
  CHECK(book.findOrder(ReferenceNum(3)) == nullptr);
  CHECK(level2->price == 102.0);
  CHECK(level2->totalShares == 100);
  CHECK(level2->numOrders() == 1);
  CHECK(&level2->front() == order2);
  CHECK(order2->refNum == ReferenceNum(2));
  CHECK(order2->quantity == 100);
  CHECK(order2->price == 102.00);
  CHECK(order2->cid == CID(1));
  CHECK(order2->side == Side::Ask);
  CHECK(order2->level == level2);
  CHECK(book.numOrders() == 2);

  // new better order on bid side, best bid is updated
  book.newOrder(ReferenceNum(3), CID(0), Side::Bid, /*Quantity*/ 100,
                /*Price*/ 101.00, Timestamp{});
  auto level3 = book.topLevel(CID(0), Side::Bid);
  REQUIRE(level3 != nullptr);
  CHECK(level3->price == 101.00);
  CHECK(level3->totalShares == 100);
  CHECK(level3->numOrders() == 1);
  auto order3 = book.findOrder(ReferenceNum(3));
  REQUIRE(order3 != nullptr);
  CHECK(&level3->front() == order3);
  CHECK(order3->refNum == ReferenceNum(3));
  CHECK(order3->quantity == 100);
  CHECK(order3->price == 101.00);
  CHECK(order3->cid == CID(0));
  CHECK(order3->side == Side::Bid);
  CHECK(order3->level == level3);
  CHECK(book.topLevel(CID(0), Side::Bid) == level3);
  CHECK(book.nthLevel(CID(0), Side::Bid, 0) == level3);
  CHECK(book.nthLevel(CID(0), Side::Bid, 1) == level1);
  CHECK(book.nthLevel(CID(0), Side::Bid, 2) == nullptr);
  CHECK(book.numOrders() == 3);

  // new worse order on ask side, best ask is not updated
  book.newOrder(ReferenceNum(4), CID(1), Side::Ask, /*Quantity*/ 100,
                /*Price*/ 103.00, Timestamp{});
  auto level4 = book.getLevel(CID(1), Side::Ask, 103.00);
  REQUIRE(level4 != nullptr);
  CHECK(level4->price == 103.00);
  CHECK(level4->totalShares == 100);
  CHECK(level4->numOrders() == 1);
  auto order4 = book.findOrder(ReferenceNum(4));
  REQUIRE(order4 != nullptr);
  CHECK(&level4->front() == order4);
  CHECK(order4->refNum == ReferenceNum(4));
  CHECK(order4->quantity == 100);
  CHECK(order4->price == 103.00);
  CHECK(order4->cid == CID(1));
  CHECK(order4->side == Side::Ask);
  CHECK(order4->level == level4);
  CHECK(book.topLevel(CID(1), Side::Ask) == level2);
  CHECK(book.nthLevel(CID(1), Side::Ask, 0) == level2);
  CHECK(book.nthLevel(CID(1), Side::Ask, 1) == level4);
  CHECK(book.nthLevel(CID(1), Side::Ask, 2) == nullptr);

  CHECK(book.numOrders() == 4);

  auto &half0 = book.half(CID(0), Side::Bid);
  REQUIRE(half0.size() == 2);
  CHECK(half0.begin()->second == level3);
  CHECK(half0.rbegin()->second == level1);

  auto &half1 = book.half(CID(1), Side::Ask);
  REQUIRE(half1.size() == 2);
  CHECK(half1.begin()->second == level2);
  CHECK(half1.rbegin()->second == level4);

  ExecInfo ei;
  book.executeOrder(ReferenceNum(1), /*Quantity*/ 10, ei, Timestamp{});
  CHECK(order1->quantity == 90);
  book.executeOrder(order1, /*Quantity*/ 10, ei, Timestamp{});
  CHECK(order1->quantity == 80);

  ei.hasPrice = true;
  ei.price = 102.01;
  book.executeOrder(ReferenceNum(2), /*Quantity*/ 10, ei, Timestamp{});
  CHECK(order2->quantity == 90);
  CHECK(order2->price == 102.00);

  book.reduceOrderBy(ReferenceNum(1), /*Quantity*/ 5, Timestamp{});
  CHECK(order1->quantity == 75);

  book.reduceOrderTo(ReferenceNum(2), /*Quantity*/ 5, Timestamp{});
  CHECK(order2->quantity == 5);

  book.deleteOrder(ReferenceNum(2), Timestamp{});
  CHECK(book.findOrder(ReferenceNum(2)) == nullptr);
  CHECK(book.getLevel(CID(1), Side::Ask, 102.00) == nullptr);
  CHECK(book.topLevel(CID(1), Side::Ask) == level4);
  CHECK(book.numOrders() == 3);

  book.replaceOrder(ReferenceNum(1), ReferenceNum(5), 80, 101.10, Timestamp{});
  CHECK(book.findOrder(ReferenceNum(1)) == nullptr);
  auto order5 = book.findOrder(ReferenceNum(5));
  REQUIRE(order5 != nullptr);
  CHECK(order5->refNum == ReferenceNum(5));
  CHECK(order5->quantity == 80);
  CHECK(order5->price == 101.10);
  CHECK(order5->cid == CID(0));
  CHECK(order5->side == Side::Bid);
  auto level5 = book.getLevel(CID(0), Side::Bid, 101.10);
  REQUIRE(level5 != nullptr);
  CHECK(level5->price == 101.10);
  CHECK(level5->totalShares == 80);
  CHECK(level5->numOrders() == 1);
  CHECK(&level5->front() == order5);
  CHECK(order5->level == level5);
  CHECK(book.topLevel(CID(0), Side::Bid) == level5);
  CHECK(book.nthLevel(CID(0), Side::Bid, 0) == level5);
  CHECK(book.nthLevel(CID(0), Side::Bid, 1) == level3);
  CHECK(book.nthLevel(CID(0), Side::Bid, 2) == nullptr);

  CHECK(book.numOrders() == 3);

  book.clearBook(CID(0));
  CHECK(book.numOrders() == 1);
  CHECK(book.topLevel(CID(0), Side::Bid) == nullptr);
  CHECK(book.topLevel(CID(1), Side::Ask) == level4);

  book.clear(true);
  CHECK(book.numOrders() == 0);
}

TEST_CASE("listener") {
  OrderBook book(BookID(1));
  CHECK(book.id() == BookID(1));

  Listener listener;
  book.addListener(&listener);

  book.resize(CID(2));
  auto order1 = book.newOrder(ReferenceNum(1), CID(0), Side::Bid, 100, 100.00, Timestamp{});
  CHECK(listener.newOrders.size() == 1);
  CHECK((listener.newOrders[0] == std::tuple(BookID(1), order1)));
  listener.newOrders.clear();

  ExecInfo ei;
  ei.hasPrice = true;
  ei.price = 100.00;
  book.executeOrder(order1, 10, ei, Timestamp{});
  CHECK(listener.execOrders.size() == 1);
  CHECK((listener.execOrders[0] == std::tuple(BookID(1), order1, 100, 10, 100.00)));
  listener.execOrders.clear();

  ei.price = 100.10;
  book.executeOrder(order1->refNum, 5, ei, Timestamp{});
  CHECK(listener.execOrders.size() == 1);
  CHECK((listener.execOrders[0] == std::tuple(BookID(1), order1, 90, 5, 100.10)));
  CHECK(order1->quantity == 85);
  CHECK(order1->price == 100.00);
  listener.execOrders.clear();

  book.reduceOrderBy(order1->refNum, 10, Timestamp{});
  CHECK(listener.updateOrders.size() == 1);
  CHECK((listener.updateOrders[0] == std::tuple(BookID(1), order1, 85, 100.00)));
  CHECK(order1->quantity == 75);
  CHECK(order1->price == 100.00);
  listener.updateOrders.clear();

  book.reduceOrderTo(order1, 10, Timestamp{});
  CHECK(listener.updateOrders.size() == 1);
  CHECK((listener.updateOrders[0] == std::tuple(BookID(1), order1, 75, 100.00)));
  CHECK(order1->quantity == 10);
  CHECK(order1->price == 100.00);
  listener.updateOrders.clear();

  auto order2 = book.replaceOrder(order1->refNum, ReferenceNum(2), 20, 100.10, Timestamp{});
  CHECK(listener.replaceOrders.size() == 1);
  CHECK((listener.replaceOrders[0] == std::tuple(BookID(1), order1, order2)));
  CHECK(order2->refNum == ReferenceNum(2));
  CHECK(order2->quantity == 20);
  CHECK(order2->price == 100.10);
  CHECK(order2->cid == CID(0));
  CHECK(order2->side == Side::Bid);
  listener.replaceOrders.clear();

  book.deleteOrder(order2->refNum, Timestamp{});
  CHECK(listener.deleteOrders.size() == 1);
  CHECK((listener.deleteOrders[0] == std::tuple(BookID(1), order2, 20)));
  listener.deleteOrders.clear();

  auto order3 = book.newOrder(ReferenceNum(3), CID(1), Side::Bid, 100, 102.00, Timestamp{});
  CHECK(listener.newOrders.size() == 1);
  CHECK((listener.newOrders[0] == std::tuple(BookID(1), order3)));
  listener.newOrders.clear();

  // replace an unknown refnum
  auto order4 = book.replaceOrder(ReferenceNum(4), ReferenceNum(5), 50, 103.00, Timestamp{});
  CHECK(order4 == nullptr);

  book.clearBook(CID(0));
  CHECK(listener.deleteOrders.size() == 0);

  book.clearBook(CID(1));
  CHECK(listener.deleteOrders.size() == 1);
  CHECK(std::find(listener.deleteOrders.begin(), listener.deleteOrders.end(),
                  std::tuple(BookID(1), order3, 100)) != listener.deleteOrders.end());
  listener.deleteOrders.clear();
  book.removeListener(&listener);

  CHECK(book.numOrders() == 0);
}

TEST_CASE("priority") {
  OrderBook book(BookID(2));
  book.resize(CID(3));
  book.newOrder(ReferenceNum(10), CID(0), Side::Bid, 100, 100.00, Timestamp{});
  book.newOrder(ReferenceNum(20), CID(0), Side::Bid, 100, 100.00, Timestamp{});
  book.newOrder(ReferenceNum(30), CID(0), Side::Bid, 100, 100.00, Timestamp{});
  book.newOrder(ReferenceNum(40), CID(0), Side::Bid, 100, 100.00, Timestamp{});
  book.newOrder(ReferenceNum(50), CID(0), Side::Bid, 100, 100.00, Timestamp{});
  CHECK(book.validate());

  auto level = book.topLevel(CID(0), Side::Bid);
  REQUIRE(level != nullptr);
  CHECK(level->price == 100.00);
  CHECK(level->totalShares == 500);
  CHECK(level->numOrders() == 5);
  CHECK(level->front().refNum == ReferenceNum(10));
  CHECK(level->back().refNum == ReferenceNum(50));
  auto oit = level->begin();
  CHECK((*oit++).refNum == ReferenceNum(10));
  CHECK((*oit++).refNum == ReferenceNum(20));
  CHECK((*oit++).refNum == ReferenceNum(30));
  CHECK((*oit++).refNum == ReferenceNum(40));
  CHECK((*oit++).refNum == ReferenceNum(50));
  CHECK(oit == level->end());

  book.replaceOrder(ReferenceNum(20), ReferenceNum(22), 100, 100.00, Timestamp{});
  CHECK(level->numOrders() == 5);
  oit = level->begin();
  CHECK((*oit++).refNum == ReferenceNum(10));
  CHECK((*oit++).refNum == ReferenceNum(30));
  CHECK((*oit++).refNum == ReferenceNum(40));
  CHECK((*oit++).refNum == ReferenceNum(50));
  CHECK((*oit++).refNum == ReferenceNum(22));
  CHECK(oit == level->end());

  book.deleteOrder(ReferenceNum(30), Timestamp{});
  book.deleteOrder(ReferenceNum(40), Timestamp{});
  book.deleteOrder(ReferenceNum(50), Timestamp{});
  CHECK(level->numOrders() == 2);
  oit = level->begin();
  CHECK((*oit++).refNum == ReferenceNum(10));
  CHECK((*oit++).refNum == ReferenceNum(22));
  CHECK(book.validate());

  book.clearBook(CID(0));
  CHECK(book.topLevel(CID(0), Side::Bid) == nullptr);
  CHECK(book.numOrders() == 0);
}

TEST_CASE("price ordering") {
  OrderBook book(BookID(3));
  book.resize(CID(4));
  book.newOrder(ReferenceNum(100), CID(0), Side::Bid, 100, 100.04, Timestamp{});
  book.newOrder(ReferenceNum(200), CID(0), Side::Bid, 100, 100.01, Timestamp{});
  book.newOrder(ReferenceNum(300), CID(0), Side::Bid, 100, 100.03, Timestamp{});
  book.newOrder(ReferenceNum(400), CID(0), Side::Bid, 100, 100.02, Timestamp{});
  book.newOrder(ReferenceNum(500), CID(0), Side::Bid, 100, 100.05, Timestamp{});

  book.newOrder(ReferenceNum(600), CID(1), Side::Ask, 100, 100.14, Timestamp{});
  book.newOrder(ReferenceNum(700), CID(1), Side::Ask, 100, 100.11, Timestamp{});
  book.newOrder(ReferenceNum(800), CID(1), Side::Ask, 100, 100.13, Timestamp{});
  book.newOrder(ReferenceNum(900), CID(1), Side::Ask, 100, 100.12, Timestamp{});
  book.newOrder(ReferenceNum(1000), CID(1), Side::Ask, 100, 100.15, Timestamp{});
  CHECK(book.validate());

  auto level = book.topLevel(CID(0), Side::Bid);
  REQUIRE(level != nullptr);
  CHECK(level->price == 100.05);

  level = book.topLevel(CID(1), Side::Ask);
  REQUIRE(level != nullptr);
  CHECK(level->price == 100.11);

  auto &half0 = book.half(CID(0), Side::Bid);
  CHECK(half0.size() == 5);

  auto &half1 = book.half(CID(1), Side::Ask);
  CHECK(half1.size() == 5);

  CHECK(book.validate());
}

TEST_CASE("erroneous input") {
  OrderBook book(BookID(4));
  CHECK(book.id() == BookID(4));

  Listener listener;
  book.addListener(&listener);

  book.resize(CID(4));
  auto order1 = book.newOrder(ReferenceNum(100), CID(0), Side::Bid, 100, 100.04, Timestamp{});
  CHECK(listener.newOrders.size() == 1);
  CHECK((listener.newOrders[0] == std::tuple(BookID(4), order1)));
  listener.newOrders.clear();

  book.reduceOrderBy(ReferenceNum(101), 10, Timestamp{});
  CHECK(listener.updateOrders.size() == 0);
  book.reduceOrderTo(ReferenceNum(101), 10, Timestamp{});
  CHECK(listener.updateOrders.size() == 0);

  book.deleteOrder(ReferenceNum(101), Timestamp{});
  CHECK(listener.deleteOrders.size() == 0);

  auto order2 = book.replaceOrder(ReferenceNum(101), ReferenceNum(102), 100, 100.04, Timestamp{});
  CHECK(order2 == nullptr);
  CHECK(listener.newOrders.size() == 0);
  CHECK(book.validate());

  book.executeOrder(ReferenceNum(100), 101, ExecInfo{.hasPrice = true, .price = 100.03},
                    Timestamp{});
  CHECK(listener.execOrders.size() == 1);
  CHECK((listener.execOrders[0] == std::tuple(BookID(4), order1, 100, 101, 100.03)));
  listener.execOrders.clear();
  CHECK(book.validate());

  // duplicate order refnum
  order2 = book.newOrder(ReferenceNum(102), CID(0), Side::Bid, 100, 100.00, Timestamp{});
  CHECK(listener.newOrders.size() == 1);
  listener.newOrders.clear();

  auto order3 = book.newOrder(ReferenceNum(102), CID(0), Side::Ask, 150, 100.04, Timestamp{});
  CHECK(listener.deleteOrders.size() == 1);
  CHECK((listener.deleteOrders[0] == std::tuple(BookID(4), order2, 100)));
  CHECK(listener.newOrders.size() == 1);
  CHECK((listener.newOrders[0] == std::tuple(BookID(4), order3)));
  listener.deleteOrders.clear();
  listener.newOrders.clear();
  CHECK(book.validate());

  // increasing order quantity
  book.reduceOrderTo(ReferenceNum(102), 160, Timestamp{});
  CHECK(listener.updateOrders.size() == 1);
  CHECK((listener.updateOrders[0] == std::tuple(BookID(4), order3, 150, 100.04)));
  listener.updateOrders.clear();
  CHECK(book.validate());

  book.reduceOrderBy(ReferenceNum(102), 200, Timestamp{});
  CHECK(listener.updateOrders.size() == 1);
  CHECK((listener.updateOrders[0] == std::tuple(BookID(4), order2, 160, 100.04)));
  listener.updateOrders.clear();
  CHECK(book.validate());
}