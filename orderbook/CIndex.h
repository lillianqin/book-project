#pragma once

#include "OrderCommon.h"
#include "hash/emhash7.h"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace bookproj {
namespace orderbook {

// CID represent the integer index of a Symbol.  Given all symbols in a book,
// we map them into contiguous integer indices starting from 0, which can be used
// as to index into arrays.
// CIndex is a bidirectional hashmap between CID and Symbol
template <typename Cid, typename SymT> struct CIndex {
  using CID = Cid;
  using Symbol = SymT;
  CIndex() : symbol2Cid{{SymT::invalid(), Cid::invalid()}} {}

  Cid findOrInsert(SymT symbol) {
    auto [it, inserted] = symbol2Cid.emplace(std::make_pair(symbol, Cid(size())));
    if (inserted) {
      if (std::cmp_greater(size(), toUnderlying(CID::max()))) [[unlikely]] {
        symbol2Cid.erase(it);
        return Cid::invalid();
      } else {
        cid2Symbol.push_back(symbol);
      }
    }
    return it->second;
  }

  SymT operator[](Cid cid) const {
    if (cid.valid() && std::cmp_less(toUnderlying(cid), cid2Symbol.size())) {
      return cid2Symbol[toUnderlying(cid)];
    }
    return SymT::invalid();
  }

  Cid operator[](SymT symbol) const {
    if (auto it = symbol2Cid.find(symbol); it != symbol2Cid.end()) {
      return it->second;
    }
    return Cid::invalid();
  }

  void reserve(size_t n) {
    cid2Symbol.reserve(n);
    symbol2Cid.reserve(n);
  }
  size_t size() const { return cid2Symbol.size(); }

private:
  std::vector<SymT> cid2Symbol;
  // note invalid is always in symbol2Cid, so it is always 1-size bigger than cid2Symbol
  emhash7::HashMap<SymT, Cid, typename SymT::Hash> symbol2Cid;
};

// The actual CID class. Most common usage is 32-bit.
template <std::signed_integral UT = int32_t> struct CIDBase {
  using underlying_type = UT;
  constexpr explicit CIDBase(underlying_type val) : value(val) {}
  constexpr explicit operator underlying_type() const { return value; }
  underlying_type operator+() const { return value; }
  bool valid() const { return value >= 0; }
  static CIDBase invalid() { return CIDBase(-1); }
  // max valid CID value
  static CIDBase max() { return CIDBase{std::numeric_limits<underlying_type>::max()}; }

  auto operator<=>(const CIDBase &rhs) const = default;

private:
  underlying_type value;
};

using CID = CIDBase<>;

} // namespace orderbook
} // namespace bookproj

namespace std {
template <std::integral UT> struct hash<bookproj::orderbook::CIDBase<UT>> {
  size_t operator()(bookproj::orderbook::CIDBase<UT> cid) const {
    using utype = bookproj::orderbook::CIDBase<UT>::underlying_type;
    return std::hash<utype>{}(utype(cid));
  }
};
} // namespace std