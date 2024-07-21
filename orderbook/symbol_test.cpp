#include "Symbol.h"
#include <catch2/catch_test_macros.hpp>
#include <cstddef>

using namespace bookproj;
using Symbol = orderbook::Symbol<8>;
using LongSymbol = orderbook::Symbol<12>;

static_assert(std::is_standard_layout_v<Symbol>);
static_assert(std::is_standard_layout_v<LongSymbol>);

TEST_CASE("basic") {
  Symbol foo("FOO");
  CHECK(foo != Symbol::invalid());
  CHECK(!(foo == Symbol("BAR")));
  CHECK(foo.valid());
  CHECK(foo.view() == "FOO");

  // memcpy to be endianess independent
  size_t hash;
  ::memcpy(&hash, "FOO\0\0\0\0", 8);
  CHECK(std::hash<Symbol>{}(foo) == Symbol::Hash{}(foo));

  // a long name will be truncated
  Symbol bar("BAR1234567890");
  CHECK(bar != Symbol::invalid());
  CHECK(bar.valid());
  CHECK(bar == Symbol("BAR12345"));
  CHECK(bar.view() == "BAR12345");

  // blank is possible
  Symbol blank("");
  CHECK(blank != Symbol::invalid());
  CHECK(blank.valid());
  CHECK(blank == Symbol(""));
  CHECK(blank.view().size() == 0);
}

TEST_CASE("long") {
  LongSymbol foo("FOO123456789");
  CHECK(foo != LongSymbol::invalid());
  CHECK(!(foo == LongSymbol("BAR")));
  CHECK(foo.valid());
  CHECK(foo.view() == "FOO123456789");

  CHECK(std::hash<LongSymbol>{}(foo) == LongSymbol::Hash{}(foo));
}