#pragma once

#include "message/message.h"
#include <algorithm>
#include <cstdint>
#include <string>

namespace bookproj {

// specfication at
// https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf
namespace itch50 {

template <typename T> using Fld = Field<T, std::endian::big>;

struct Price4 {
  constexpr explicit operator double() const { return value * 1e-4; }
  constexpr double operator+() const { return value * 1e-4; }
  uint32_t value;

  std::string toString() const;
};

struct Price8 {
  constexpr explicit operator double() const { return value * 1e-8; }
  constexpr double operator+() const { return value * 1e-8; }

  uint64_t value;

  std::string toString() const;
};

// stock name needs to remove the trailing white spaces
static inline constexpr std::string_view stockName(const char (&stock)[8]) {
  return std::string_view(std::begin(stock), std::find(std::begin(stock), std::end(stock), ' '));
}

// some alpha names are all characters
template <size_t N> inline constexpr std::string_view alphaName(const char (&alpha)[N]) {
  return std::string_view(std::begin(alpha), std::end(alpha));
}

static inline constexpr uint64_t nanosSinceMidnight(const unsigned char (&timestamp)[6]) {
  // big endian
  return ((uint64_t(timestamp[0]) << 40) | (uint64_t(timestamp[1]) << 32)) |
         ((uint64_t(timestamp[2]) << 24) | (uint64_t(timestamp[3]) << 16)) |
         ((uint64_t(timestamp[4]) << 8) | timestamp[5]);
}

struct CommonHeader {
  CommonHeader(char c) : messageType(c) {}

  char messageType;
  Fld<uint16_t> stockLocate;
  Fld<uint16_t> trackingNumber;
  unsigned char timestamp[6];

  std::string toString() const;
};

static_assert(sizeof(CommonHeader) == 11);

struct SystemEvent {
  CommonHeader header{'S'};
  char eventCode;

  std::string toString() const;
};

static_assert(sizeof(SystemEvent) == 12);

struct StockDirectory {
  CommonHeader header{'R'};
  char stock[8];
  char marketCategory;
  char financialStatusIndicator;
  Fld<uint32_t> roundLotSize;
  char roundLotsOnly;
  char issueClassification;
  char issueSubType[2];
  char authenticity;
  char shortSaleThresholdIndicator;
  char ipoFlag;
  char luldReferencePriceTier;
  char etpFlag;
  Fld<uint32_t> etpLeverageFactor;
  char inverseIndicator;

  std::string toString() const;
};
static_assert(sizeof(StockDirectory) == 39);

struct StockTradingAction {
  CommonHeader header{'H'};
  char stock[8];
  char tradingState;
  char reserved;
  char reason[4];

  std::string toString() const;
};
static_assert(sizeof(StockTradingAction) == 25);

struct RegShoRestriction {
  CommonHeader header{'Y'};
  char stock[8];
  char regSHOAction;

  std::string toString() const;
};
static_assert(sizeof(RegShoRestriction) == 20);

struct MarketParticipantPosition {
  CommonHeader header{'L'};
  char mpId[4];
  char stock[8];
  char primaryMarketMaker;
  char marketMakerMode;
  char marketParticipantState;

  std::string toString() const;
};
static_assert(sizeof(MarketParticipantPosition) == 26);

struct MWCBDeclineLevel {
  CommonHeader header{'V'};
  Fld<Price8> level1;
  Fld<Price8> level2;
  Fld<Price8> level3;

  std::string toString() const;
};
static_assert(sizeof(MWCBDeclineLevel) == 35);

struct MWCBStatus {
  CommonHeader header{'W'};
  char breachLevel;

  std::string toString() const;
};
static_assert(sizeof(MWCBStatus) == 12);

struct QuotingPeriodUpdate {
  CommonHeader header{'K'};
  char stock[8];
  Fld<uint32_t> ipoQuotationReleaseTime;
  char ipoQuotationReleaseQualifier;
  Fld<Price4> ipoPrice;

  std::string toString() const;
};
static_assert(sizeof(QuotingPeriodUpdate) == 28);

struct LULDAuctionCollar {
  CommonHeader header{'J'};
  char stock[8];
  Fld<Price4> auctionCollarReferencePrice;
  Fld<Price4> upperAuctionCollarPrice;
  Fld<Price4> lowerAuctionCollarPrice;
  Fld<uint32_t> auctionCollarExtension;

  std::string toString() const;
};
static_assert(sizeof(LULDAuctionCollar) == 35);

struct OperationalHalt {
  CommonHeader header{'h'};
  char stock[8];
  char marketCode;
  char operationalHaltAction;

  std::string toString() const;
};
static_assert(sizeof(OperationalHalt) == 21);

struct AddOrder {
  CommonHeader header{'A'};
  Fld<uint64_t> orderReferenceNumber;
  char buySellIndicator;
  Fld<uint32_t> shares;
  char stock[8];
  Fld<Price4> price;

  std::string toString() const;
};
static_assert(sizeof(AddOrder) == 36);

struct AddOrderMPID {
  CommonHeader header{'F'};
  Fld<uint64_t> orderReferenceNumber;
  char buySellIndicator;
  Fld<uint32_t> shares;
  char stock[8];
  Fld<Price4> price;
  char attribution[4];
  std::string toString() const;
};
static_assert(sizeof(AddOrderMPID) == 40);

struct OrderExecuted {
  CommonHeader header{'E'};
  Fld<uint64_t> orderReferenceNumber;
  Fld<uint32_t> executedShares;
  Fld<uint64_t> matchNumber;

  std::string toString() const;
};
static_assert(sizeof(OrderExecuted) == 31);

struct OrderExecutedWithPrice {
  CommonHeader header{'C'};
  Fld<uint64_t> orderReferenceNumber;
  Fld<uint32_t> executedShares;
  Fld<uint64_t> matchNumber;
  char printable;
  Fld<Price4> executionPrice;

  std::string toString() const;
};
static_assert(sizeof(OrderExecutedWithPrice) == 36);

struct OrderCancel {
  CommonHeader header{'X'};
  Fld<uint64_t> orderReferenceNumber;
  Fld<uint32_t> canceledShares;

  std::string toString() const;
};
static_assert(sizeof(OrderCancel) == 23);

struct OrderDelete {
  CommonHeader header{'D'};
  Fld<uint64_t> orderReferenceNumber;

  std::string toString() const;
};
static_assert(sizeof(OrderDelete) == 19);

struct OrderReplace {
  CommonHeader header{'U'};
  Fld<uint64_t> originalOrderReferenceNumber;
  Fld<uint64_t> newOrderReferenceNumber;
  Fld<uint32_t> shares;
  Fld<Price4> price;

  std::string toString() const;
};
static_assert(sizeof(OrderReplace) == 35);

struct Trade {
  CommonHeader header{'P'};
  Fld<uint64_t> orderReferenceNumber;
  char buySellIndicator;
  Fld<uint32_t> shares;
  char stock[8];
  Fld<Price4> price;
  Fld<uint64_t> matchNumber;

  std::string toString() const;
};
static_assert(sizeof(Trade) == 44);

struct CrossTrade {
  CommonHeader header{'Q'};
  Fld<uint64_t> shares;
  char stock[8];
  Fld<Price4> crossPrice;
  Fld<uint64_t> matchNumber;
  char crossType;

  std::string toString() const;
};
static_assert(sizeof(CrossTrade) == 40);

struct BrokenTrade {
  CommonHeader header{'B'};
  Fld<uint64_t> matchNumber;

  std::string toString() const;
};
static_assert(sizeof(BrokenTrade) == 19);

struct NOII {
  CommonHeader header{'I'};
  Fld<uint64_t> pairedShares;
  Fld<uint64_t> imbalanceShares;
  char imbalanceDirection;
  char stock[8];
  Fld<Price4> farPrice;
  Fld<Price4> nearPrice;
  Fld<Price4> currentReferencePrice;
  char crossType;
  char priceVariationIndicator;

  std::string toString() const;
};
static_assert(sizeof(NOII) == 50);

struct RPII {
  CommonHeader header{'N'};
  char stock[8];
  char interestFlag;

  std::string toString() const;
};
static_assert(sizeof(RPII) == 20);

struct DirectListingWithCapitalRaisePriceDiscovery {
  CommonHeader header{'O'};
  char stock[8];
  char openEligibilityStatus;
  Fld<Price4> minimumAllowedPrice;
  Fld<Price4> maximumAllowedPrice;
  Fld<Price4> nearExecutionPrice;
  Fld<uint64_t> nearExecutionTime;
  Fld<Price4> lowerPriceRangeCollar;
  Fld<Price4> upperPriceRangeCollar;

  std::string toString() const;
};
static_assert(sizeof(DirectListingWithCapitalRaisePriceDiscovery) == 48);

} // namespace itch50

} // namespace bookproj