#include "CIndex.h"
#include "Symbol.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <string>
#include <utility>

using namespace bookproj;
using CID = orderbook::CID;
using Symbol = orderbook::Symbol<8>;
using CIndex = orderbook::CIndex<orderbook::CID, orderbook::Symbol<8>>;

TEST_CASE("basic") {
  CIndex cindex;
  CHECK(cindex.size() == 0);
  CHECK(cindex[CID{0}] == Symbol::invalid());
  CHECK(cindex[Symbol("FOO")] == CID::invalid());
  CHECK(cindex[CID::invalid()] == Symbol::invalid());
  CHECK(cindex[Symbol::invalid()] == CID::invalid());

  CID cid1 = cindex.findOrInsert(Symbol("FOO"));
  CHECK(cid1 == CID{0});
  CHECK(cindex.size() == 1);
  CHECK(cindex[cid1] == Symbol("FOO"));
  CHECK(cindex[Symbol("FOO")] == cid1);
  CHECK(cindex[Symbol("BAR")] == CID::invalid());

  CID cid2 = cindex.findOrInsert(Symbol("BAR"));
  CHECK(cid2 == CID{1});
  CHECK(cindex.size() == 2);
  CHECK(cindex[cid2] == Symbol("BAR"));
  CHECK(cindex[Symbol("BAR")] == cid2);
  CHECK(cindex[Symbol("FOO")] == cid1);

  CID cid3 = cindex.findOrInsert(Symbol("FOO"));
  CHECK(cid3 == cid1);
}

using CIDNarrow = orderbook::CIDBase<int8_t>;
using CIndexNarrow = orderbook::CIndex<CIDNarrow, Symbol>;

TEST_CASE("overflow") {
  CIndexNarrow cindex;
  cindex.reserve(2);
  for (int i = 0; i < +CIDNarrow::max(); ++i) {
    auto cid = cindex.findOrInsert(std::string_view(std::to_string(i)));
    CHECK(cid.valid());
    CHECK(cid < CIDNarrow::max());
  }
  auto cid = cindex.findOrInsert(Symbol("FOO"));
  CHECK(cid == CIDNarrow::max());
  CHECK(std::cmp_equal(cindex.size(), +CIDNarrow::max() + 1));

  auto invalid = cindex.findOrInsert(Symbol("BAR"));
  CHECK(invalid == CIDNarrow::invalid());
  CHECK(std::cmp_equal(cindex.size(), +CIDNarrow::max() + 1));
}
