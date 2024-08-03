#include "OrderBook.h"
#include "itch50.h"
#include "itch50HistDataSource.h"
#include "itch50OrderBook.h"
#include "itch50RawParser.h"
#include "orderbook/OrderBookPrinter.h"
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/log/log.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace bookproj {
namespace itch50 {

using NYTime = std::chrono::local_time<std::chrono::nanoseconds>;

NYTime toNYTime(Timestamp ts) {
  static const std::chrono::time_zone *zone = std::chrono::locate_zone("America/New_York");
  return zone->to_local(ts);
}

// handler for non book modifying update
struct Itch50NBMUpdateHandler {
  Itch50NBMUpdateHandler(const CIndex &cindex_, const StockLocateMap &lindex_, Timestamp midnight_,
                         Timestamp startTime_, bool printNoSymEvents_)
      : cindex(cindex_), lindex(lindex_), midnight(midnight_), startTime(startTime_),
        printNoSym(printNoSymEvents_) {}

  void process(const Trade &msg) {
    auto ts = getTimestamp(msg.header);
    if (ts >= startTime) {
      auto cid = lindex[StockLocate(+msg.header.stockLocate)];
      if (cid.valid()) {
        std::cout << std::format(
            "{:%Y%m%d %H:%M:%S} {} onTrade refnum={} side={} sz={} px={:.4f} matchnum={}\n",
            toNYTime(ts), cindex[cid].view(), +msg.orderReferenceNumber, msg.buySellIndicator,
            +msg.shares, +msg.price.value(), +msg.matchNumber);
      }
    }
  }

  void process(const CrossTrade &msg) {
    auto ts = getTimestamp(msg.header);
    if (ts >= startTime) {
      auto cid = lindex[StockLocate(+msg.header.stockLocate)];
      if (cid.valid()) {
        std::cout << std::format(
            "{:%Y%m%d %H:%M:%S} {} onCrossTrade type={} sz={} px={:.4f} matchnum={}\n",
            toNYTime(ts), cindex[cid].view(), msg.crossType, +msg.shares, +msg.crossPrice.value(),
            +msg.matchNumber);
        std::cout << msg.toString() << '\n';
      }
    }
  }

  void process(const NOII &msg) {
    auto ts = getTimestamp(msg.header);
    if (ts >= startTime) {
      auto cid = lindex[StockLocate(+msg.header.stockLocate)];
      if (cid.valid()) {
        std::cout << std::format("{:%Y%m%d %H:%M:%S} {} onNOII type={} dir={} paired={} imbal={} "
                                 "refpx={:.4f} nearpx={:.4f} farpx={:.4f} pxvar={}\n",
                                 toNYTime(ts), cindex[cid].view(), msg.crossType,
                                 msg.imbalanceDirection, +msg.pairedShares, +msg.imbalanceShares,
                                 +msg.currentReferencePrice.value(), +msg.nearPrice.value(),
                                 +msg.farPrice.value(), msg.priceVariationIndicator);
      }
    }
  }

  void process(const StockTradingAction &msg) {
    auto ts = getTimestamp(msg.header);
    if (ts >= startTime) {
      auto cid = lindex[StockLocate(+msg.header.stockLocate)];
      if (cid.valid()) {
        std::cout << std::format("{:%Y%m%d %H:%M:%S} {} onStockTradingAction state={} reason={}\n",
                                 toNYTime(ts), cindex[cid].view(), msg.tradingState,
                                 alphaName(msg.reason));
      }
    }
  }

  void process(const RegShoRestriction &msg) {
    auto ts = getTimestamp(msg.header);
    if (ts >= startTime) {
      auto cid = lindex[StockLocate(+msg.header.stockLocate)];
      if (cid.valid()) {
        std::cout << std::format("{:%Y%m%d %H:%M:%S} {} onRegShoRestriction action={}\n",
                                 toNYTime(ts), cindex[cid].view(), msg.regSHOAction);
      }
    }
  }

  void process(const SystemEvent &msg) {
    if (printNoSym) {
      auto ts = getTimestamp(msg.header);
      if (ts >= startTime) {
        std::cout << std::format("{:%Y%m%d %H:%M:%S} onSystemEvent event={}\n", toNYTime(ts),
                                 msg.eventCode);
      }
    }
  }

  // catchall, does nothing for other messages
  template <typename Msg> void process(const Msg &) {};

  Timestamp getTimestamp(const CommonHeader &header) const {
    return midnight + std::chrono::nanoseconds(nanosSinceMidnight(header.timestamp));
  }

  const CIndex &cindex;
  const StockLocateMap &lindex;

  Timestamp midnight;
  Timestamp startTime;
  bool printNoSym;
};

struct Listener : public orderbook::BookListener {
  using Book = orderbook::OrderBook;
  using Order = orderbook::Order;
  using RefNum = orderbook::ReferenceNum;
  using Size = orderbook::Quantity;
  using Price = orderbook::Price;
  using CID = orderbook::CID;
  using Side = orderbook::Side;
  using Quantity = orderbook::Quantity;
  using BookID = orderbook::BookID;

  Listener(const Book &book_, const CIndex &cindex_, Timestamp start_, int depth_)
      : book(book_), cindex(cindex_), startTime(start_), depth(depth_) {}

  void onNewOrder(BookID book, const Order *order) override {
    if (order->createTime >= startTime) {
      std::cout << std::format("{:%Y%m%d %H:%M:%S} {} onNewOrder {}\n",
                               toNYTime(order->createTime), cindex[order->cid].view(),
                               order->toString());
      printBook(order->cid, order->updateTime);
    }
  }

  void onDeleteOrder(BookID book, const Order *order, Quantity oldQuantity) override {
    if (order->updateTime >= startTime) {
      std::cout << std::format("{:%Y%m%d %H:%M:%S} {} onDeleteOrder {} origsz={} \n",
                               toNYTime(order->updateTime), cindex[order->cid].view(),
                               order->toString(), orderbook::toUnderlying(oldQuantity));
      printBook(order->cid, order->updateTime);
    }
  }

  void onReplaceOrder(BookID book, const Order *order, const Order *oldOrder) override {
    if (order->updateTime >= startTime) {
      std::cout << std::format("{:%Y%m%d %H:%M:%S} {} onReplaceOrder new: {} orig: {}\n",
                               toNYTime(order->updateTime), cindex[order->cid].view(),
                               order->toString(), oldOrder->toString());
      printBook(order->cid, order->updateTime);
    }
  }

  void onExecOrder(BookID book, const Order *order, Quantity oldQuantity, Quantity fillQuantity,
                   const orderbook::ExecInfo &ei) override {
    if (order->updateTime >= startTime) {
      std::cout << std::format(
          "{:%Y%m%d %H:%M:%S} {} onExecOrder {} {} origsz={} fillsz={}\n",
          toNYTime(order->updateTime), cindex[order->cid].view(), order->toString(), ei.toString(),
          orderbook::toUnderlying(oldQuantity), orderbook::toUnderlying(fillQuantity));
      printBook(order->cid, order->updateTime);
    }
  }

  void onUpdateOrder(BookID book, const Order *order, Quantity oldQuantity,
                     Price oldPrice) override {
    if (order->updateTime >= startTime) {
      std::cout << std::format("{:%Y%m%d %H:%M:%S} {} onUpdateOrder {} origsz={} origpx={:.4f} \n",
                               toNYTime(order->updateTime), cindex[order->cid].view(),
                               order->toString(), orderbook::toUnderlying(oldQuantity),
                               double(oldPrice));
      printBook(order->cid, order->updateTime);
    }
  }

  void printBook(CID cid, Timestamp ts) {
    if (depth > 0) {
      orderbook::PrintParams params{
          .orderWidth = 4, .quantityWidth = 6, .priceWidth = 10, .pricePrecision = 4};
      auto levels = orderbook::printLevels(book, cid, depth, params);
      for (const auto &level : levels) {
        std::cout << level << "\n";
      }
    }
  }

private:
  const Book &book;
  const CIndex &cindex;
  const Timestamp startTime;
  const int depth;
};

} // namespace itch50
} // namespace bookproj

using bookproj::datasource::Itch50HistDataSource;
using bookproj::itch50::CIndex;
using bookproj::itch50::StockLocateMap;
using bookproj::itch50::Symbol;
using bookproj::itch50::Timestamp;
using bookproj::orderbook::BookID;
using bookproj::orderbook::CID;

using Listener = bookproj::itch50::Listener;
using Book = bookproj::orderbook::OrderBook;
using NBMHandler = bookproj::itch50::Itch50NBMUpdateHandler;
using QuoteHandler = bookproj::itch50::Itch50QuoteHandler;
using SymbolHandler = bookproj::itch50::Itch50SymbolHandler;

ABSL_FLAG(int32_t, date, 0, "date of the input itch file, as yyyymmdd");
ABSL_FLAG(bool, printUpdate, true, "print book updates");
ABSL_FLAG(int32_t, depth, 0, "depth of book to print on each update");
ABSL_FLAG(bool, printOther, true, "print non-book modifying updates");
ABSL_FLAG(std::string, startTime, "00:00:00", "start time to print, HH:MM:SS.usec");
ABSL_FLAG(std::string, endTime, "23:59:59", "stop time, HH:MM:SS.usec");
ABSL_FLAG(std::vector<std::string>, symbols, {}, "symbols to print");

Timestamp::duration parseStringToDuration(const std::string &str) {
  // TODO: update when std::chrono::from_stream is supported
  int hour, minute;
  double sec = 0.0;
  if (std::sscanf(str.c_str(), "%2d:%2d:%lf", &hour, &minute, &sec) >= 2 && hour >= 0 &&
      hour < 24 && minute >= 0 && minute < 60 && sec >= 0 && sec < 60) {
    return std::chrono::hours(hour) + std::chrono::minutes(minute) +
           std::chrono::duration_cast<Timestamp::duration>(std::chrono::duration<double>(sec));
  }
  std::cerr << "Error parsing time: " << str << std::endl;
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  absl::SetProgramUsageMessage("Utility to print nasdaq itch50 books for given date");

  auto remains = absl::ParseCommandLine(argc, argv);
  if (remains.size() != 1) {
    std::cerr << "Error: unexpected command line argument " << remains.back() << "\n";
    return 1;
  }
  int date = absl::GetFlag(FLAGS_date);
  if (date == 0) {
    std::cerr << "Error: a valid date must be provided must be provided via --date\n";
    return 1;
  }
  Timestamp midnight = Itch50HistDataSource::midnightNYTime(date);
  Timestamp start = midnight + parseStringToDuration(absl::GetFlag(FLAGS_startTime));
  Timestamp::duration end = parseStringToDuration(absl::GetFlag(FLAGS_endTime));

  std::cerr << std::format("start={:%Y%m%d %H:%M:%S} end={:%H:%M:%S}\n",
                           bookproj::itch50::toNYTime(start),
                           bookproj::itch50::toNYTime(midnight + end));
  Book book(BookID{0});
  book.reserve(65535, 4 << 20, 2 << 19);
  book.resize(CID(65535));
  static_assert(sizeof(Book::OrderExt) == 72);
  static_assert(sizeof(Book::Level) == 64);

  // quote/misc handlers use StockLocateMap to filter symbols
  StockLocateMap stockLocateMap;
  CIndex cindex;
  for (const auto &symbol : absl::GetFlag(FLAGS_symbols)) {
    cindex.findOrInsert(Symbol(symbol));
  }
  bool addAllSymbols = cindex.size() == 0;

  Listener listener(book, cindex, start, absl::GetFlag(FLAGS_depth));
  if (absl::GetFlag(FLAGS_printUpdate)) {
    book.addListener(&listener);
  }

  SymbolHandler symbolHandler(cindex, stockLocateMap, addAllSymbols);
  QuoteHandler quoteHandler(book, stockLocateMap, midnight, addAllSymbols);
  NBMHandler miscHandler(cindex, stockLocateMap, midnight,
                         absl::GetFlag(FLAGS_printOther) ? start : Timestamp::max(),
                         addAllSymbols);

  Itch50HistDataSource::setRootPath("/opt/data");
  std::unique_ptr<Itch50HistDataSource> source;
  try {
    source.reset(new Itch50HistDataSource(date));
    source->setEndTime(midnight + end);
  } catch (const std::runtime_error &e) {
    std::cerr << "Error creating data source: " << e.what() << std::endl;
    return 1;
  }
  while (source->hasMessage()) {
    auto result = bookproj::itch50::parseMessage(source->nextMessage(), symbolHandler,
                                                 quoteHandler, miscHandler);
    if (result != bookproj::itch50::ParseResultType::Success) [[unlikely]] {
      std::cerr << "Error parsing message: " << bookproj::itch50::toString(result)
                << " file offset: " << source->currentOffset() << " time: "
                << std::format("{:%Y%m%d %H:%M:%S}",
                               bookproj::itch50::toNYTime(source->nextTime()))
                << std::endl;
      break;
    }
    source->advance();
  }
  std::cerr << "done processing book, remaining orders=" << book.numOrders()
            << ", remaining levels=" << book.numLevels() << '\n';
  std::cerr << "maxNumOrders=" << book.maxNumOrders() << ", maxNumLevels=" << book.maxNumLevels()
            << "\n";

  book.removeListener(&listener);
  return 0;
}