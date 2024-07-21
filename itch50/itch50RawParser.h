#pragma once

#include "itch50.h"
#include <limits>
#include <span>

namespace bookproj {
namespace itch50 {

enum class ParseResultType { Success, BadMsgType, BadSize };

std::string toString(ParseResultType type);

template <typename... Args>
ParseResultType parseMessage(std::span<const std::byte> msg, Args &...handler) {
  const char *data = reinterpret_cast<const char *>(msg.data());

  const CommonHeader *header = std::launder(reinterpret_cast<const CommonHeader *>(data));
  char typeChar = header->messageType;
  size_t expectedSz = std::numeric_limits<size_t>::max();
  switch (typeChar) {
#define CASE(c, MsgType)                                                                          \
  case c: {                                                                                       \
    expectedSz = sizeof(MsgType);                                                                 \
    if (expectedSz <= msg.size()) {                                                               \
      const MsgType *msg = std::launder(reinterpret_cast<const MsgType *>(data));                 \
      (handler.process(*msg), ...);                                                               \
    }                                                                                             \
    break;                                                                                        \
  }
    CASE('S', SystemEvent);
    CASE('R', StockDirectory);
    CASE('H', StockTradingAction);
    CASE('Y', RegShoRestriction);
    CASE('L', MarketParticipantPosition);
    CASE('V', MWCBDeclineLevel);
    CASE('W', MWCBStatus);
    CASE('K', QuotingPeriodUpdate);
    CASE('J', LULDAuctionCollar);
    CASE('h', OperationalHalt);
    CASE('A', AddOrder);
    CASE('F', AddOrderMPID);
    CASE('E', OrderExecuted);
    CASE('C', OrderExecutedWithPrice);
    CASE('X', OrderCancel);
    CASE('D', OrderDelete);
    CASE('U', OrderReplace);
    CASE('P', Trade);
    CASE('Q', CrossTrade);
    CASE('B', BrokenTrade);
    CASE('I', NOII);
    CASE('N', RPII);
    CASE('O', StockTradingAction);

#undef CASE
  default:
    break;
  }

  // we allow expectedSz to be smaller in case message has new extension
  if (expectedSz > msg.size()) [[unlikely]] {
    if (expectedSz == std::numeric_limits<size_t>::max()) {
      return ParseResultType::BadMsgType;
    } else {
      return ParseResultType::BadSize;
    }
  }
  return ParseResultType::Success;
} // namespace itch50

} // namespace itch50
} // namespace bookproj
