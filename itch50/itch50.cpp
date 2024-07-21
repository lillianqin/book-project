#include "itch50.h"
#include <format>
#include <string>

namespace bookproj {
namespace itch50 {

std::string timestampToString(const unsigned char (&timestamp)[6]) {
  uint64_t nanos = nanosSinceMidnight(timestamp);
  int hours = nanos / 3'600'000'000'000;
  int minutes = (nanos / 60'000'000'000) % 60;
  int seconds = (nanos / 1'000'000'000) % 60;
  int nanoseconds = nanos % 1'000'000'000;
  return std::format("{:02d}:{:02d}:{:02d}:{:09d}", hours, minutes, seconds, nanoseconds);
}

std::string Price4::toString() const {
  return std::format("{}.{:04d}", value / 10000, value % 10000);
}

std::string Price8::toString() const {
  return std::format("{}.{:08d}", value / 100000000, value % 100000000);
}

std::string CommonHeader::toString() const {
  return std::format("messageType={} stockLoate={} trackingNumber={}", messageType,
                     stockLocate.value(), trackingNumber.value());
}

std::string SystemEvent::toString() const {
  return std::format("{} SystemEvent {} eventCode={}", timestampToString(header.timestamp),
                     header.toString(), eventCode);
}

std::string StockDirectory::toString() const {
  return std::format(
      "{} StockDirectory {} stock={} marketCategory={} financialStatusIndicator={} "
      "roundLotSize={} "
      "roundLotsOnly={} issueClassification={} issueSubType={} authenticity={} "
      "shortSaleThresholdIndicator={} ipoFlag={} luldReferencePriceTier={}, etpFlag={} "
      "etpLeverageFactor={} inverseIndicator={}",
      timestampToString(header.timestamp), header.toString(), stockName(stock), marketCategory,
      financialStatusIndicator, roundLotSize.value(), roundLotsOnly, issueClassification,
      alphaName(issueSubType), authenticity, shortSaleThresholdIndicator, ipoFlag,
      luldReferencePriceTier, etpFlag, etpLeverageFactor.value(), inverseIndicator);
}

std::string StockTradingAction::toString() const {
  return std::format("{} StockTradingAction {} stock={} tradingState={} reserved={} reason={}",
                     timestampToString(header.timestamp), header.toString(), stockName(stock),
                     tradingState, reserved, alphaName(reason));
}

std::string RegShoRestriction::toString() const {
  return std::format("{} RegShoRestriction {} stock={} regSHOAction={}",
                     timestampToString(header.timestamp), header.toString(), stockName(stock),
                     regSHOAction);
}

std::string MarketParticipantPosition::toString() const {
  return std::format(
      "{} MarketParticipantPosition {} mpid={} stock={} primaryMarketMaker={} marketMakerMode={} "
      "marketParticipantState={}",
      timestampToString(header.timestamp), header.toString(), alphaName(mpId), stockName(stock),
      primaryMarketMaker, marketMakerMode, marketParticipantState);
}

std::string MWCBDeclineLevel::toString() const {
  return std::format("{} MWCBDeclineLevel {} level1={} level2={} level3={}",
                     timestampToString(header.timestamp), header.toString(),
                     level1.value().toString(), level2.value().toString(),
                     level3.value().toString());
}

std::string MWCBStatus::toString() const {
  return std::format("{} MWCBStatus {} breachLevel={} ", timestampToString(header.timestamp),
                     header.toString(), breachLevel);
}

std::string QuotingPeriodUpdate::toString() const {
  uint32_t releaseTime = ipoQuotationReleaseTime.value();
  uint32_t hours = releaseTime / 3600;
  uint32_t minutes = (releaseTime / 60) % 60;
  uint32_t seconds = releaseTime % 60;
  return std::format(
      "{} QuotingPeriodUpdate {} stock={} ipoQuotationReleaseTime={:02d}:{:02d}:{:02d} "
      "ipoQuotationReleaseQualifier={} ipoPrice={}",
      timestampToString(header.timestamp), header.toString(), stockName(stock), hours, minutes,
      seconds, ipoQuotationReleaseQualifier, ipoPrice.value().toString());
}

std::string LULDAuctionCollar::toString() const {
  return std::format(
      "{} LULDAuctionCollar {} stock={} auctionCollarReferencePrice={} "
      "upperAuctionCollarPrice={} lowerAuctionCollarPrice={} auctionCollarExtension={}",
      timestampToString(header.timestamp), header.toString(), stockName(stock),
      auctionCollarReferencePrice.value().toString(), upperAuctionCollarPrice.value().toString(),
      lowerAuctionCollarPrice.value().toString(), auctionCollarExtension.value());
}

std::string OperationalHalt::toString() const {
  return std::format("{} OperationalHalt {} stock={} marketCode={} operationalHaltAction={}",
                     timestampToString(header.timestamp), header.toString(), stockName(stock),
                     marketCode, operationalHaltAction);
}

std::string AddOrder::toString() const {
  return std::format(
      "{} AddOrder {} orderReferenceNumber={} buySellIndicator={} shares={} stock={} price={}",
      timestampToString(header.timestamp), header.toString(), orderReferenceNumber.value(),
      buySellIndicator, shares.value(), stockName(stock), price.value().toString());
}

std::string AddOrderMPID::toString() const {
  return std::format("{} AddOrderMPID {} orderReferenceNumber={} buySellIndicator={} shares={} "
                     "stock={} price={} attribution={}",
                     timestampToString(header.timestamp), header.toString(),
                     orderReferenceNumber.value(), buySellIndicator, shares.value(),
                     stockName(stock), price.value().toString(), alphaName(attribution));
}

std::string OrderExecuted::toString() const {
  return std::format(
      "{} OrderExecuted {} orderReferenceNumber={} executedShares={} matchNumber={}",
      timestampToString(header.timestamp), header.toString(), orderReferenceNumber.value(),
      executedShares.value(), matchNumber.value());
}

std::string OrderExecutedWithPrice::toString() const {
  return std::format("{} OrderExecutedWithPrice {} orderReferenceNumber={} executedShares={} "
                     "matchNumber={} printable={} executionPrice={}",
                     timestampToString(header.timestamp), header.toString(),
                     orderReferenceNumber.value(), executedShares.value(), matchNumber.value(),
                     printable, executionPrice.value().toString());
}

std::string OrderCancel::toString() const {
  return std::format("{} OrderCancel {} orderReferenceNumber={} canceledShares={}",
                     timestampToString(header.timestamp), header.toString(),
                     orderReferenceNumber.value(), canceledShares.value());
}

std::string OrderDelete::toString() const {
  return std::format("{} OrderDelete {} orderReferenceNumber={}",
                     timestampToString(header.timestamp), header.toString(),
                     orderReferenceNumber.value());
}

std::string OrderReplace::toString() const {
  return std::format(
      "{} OrderReplace {} originalOrderReferenceNumber={} newOrderReferenceNumber={} "
      "shares={} price={}",
      timestampToString(header.timestamp), header.toString(), originalOrderReferenceNumber.value(),
      newOrderReferenceNumber.value(), shares.value(), price.value().toString());
}

std::string Trade::toString() const {
  return std::format("{} Trade {} orderReferenceNumber={} buySellIndicator={} shares={} stock={} "
                     "price={} matchNumber={}",
                     timestampToString(header.timestamp), header.toString(),
                     orderReferenceNumber.value(), buySellIndicator, shares.value(),
                     stockName(stock), price.value().toString(), matchNumber.value());
}

std::string CrossTrade::toString() const {
  return std::format(
      "{} CrossTrade {} shares={} stock={} crossPrice={} matchNumber={} crossType={}",
      timestampToString(header.timestamp), header.toString(), shares.value(), stockName(stock),
      crossPrice.value().toString(), matchNumber.value(), crossType);
}

std::string BrokenTrade::toString() const {
  return std::format("{} BrokenTrade {} matchNumber={}", timestampToString(header.timestamp),
                     header.toString(), matchNumber.value());
}

std::string NOII::toString() const {
  return std::format(
      "{} NOII {} pairedShares={} imbalanceShares={} stock={} farPrice={} nearPrice={} "
      "currentReferencePrice={} crossType={} priceVariationIndicator={}",
      timestampToString(header.timestamp), header.toString(), pairedShares.value(),
      imbalanceShares.value(), stockName(stock), farPrice.value().toString(),
      nearPrice.value().toString(), currentReferencePrice.value().toString(), crossType,
      priceVariationIndicator);
}

std::string RPII::toString() const {
  return std::format("{} RPII {} stock={} interestFlag={}", timestampToString(header.timestamp),
                     header.toString(), stockName(stock), interestFlag);
}

std::string DirectListingWithCapitalRaisePriceDiscovery::toString() const {
  return std::format(
      "{} DirectListingWithCapitalRaisePriceDiscovery {} stock={} openEligibilityStatus={} "
      "minimumAllowedPrice={} maximumAllowedPrice={} nearExecutionPrice={} nearExecutionTime={} "
      "lowerPriceRangeCollar={} upperPriceRangeCollar={}",
      timestampToString(header.timestamp), header.toString(), stockName(stock),
      openEligibilityStatus, minimumAllowedPrice.value().toString(),
      maximumAllowedPrice.value().toString(), nearExecutionPrice.value().toString(),
      nearExecutionTime.value(), lowerPriceRangeCollar.value().toString(),
      upperPriceRangeCollar.value().toString());
}

} // namespace itch50
} // namespace bookproj
