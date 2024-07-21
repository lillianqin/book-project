#include "message.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <sys/types.h>

using namespace bookproj;
#pragma GCC diagnostic error "-Wpadded"

template <std::endian endianess = std::endian::native> struct MessageHeader {
  constexpr MessageHeader() noexcept = default;
  constexpr MessageHeader(uint16_t msgid, uint16_t size) noexcept : msgid(msgid), size(size) {}
  Field<uint16_t, endianess> msgid;
  Field<uint16_t, endianess> size;
};

static_assert(std::is_standard_layout_v<MessageHeader<std::endian::native>>);
static_assert(std::is_trivially_copyable_v<MessageHeader<std::endian::native>>);

struct PlainMessage : MessageBase<PlainMessage> {
  MessageHeader<> header{1, sizeof(PlainMessage)};
  Field<uint8_t> a;
  Field<double> b;
  Field<int> c;

  struct Bits {
    uint8_t a : 1;
    uint8_t b : 7;
  };
  Field<Bits> d;

  Field<uint16_t> e[2];
  std::array<char, 8> f;
  std::array<unsigned char, 8> g[2];
  Field<std::byte[8]> h[];
};

TEST_CASE("Plain Message") {
  std::byte bytes[sizeof(PlainMessage) + 16];
  PlainMessage *pmsg = new (bytes) PlainMessage();
  PlainMessage &msg = *pmsg;

  CHECK(msg.header.msgid == 1);
  CHECK(msg.header.size == sizeof(PlainMessage));
  CHECK(msg.getMsgSize() == sizeof(PlainMessage));

  msg.a = 11;
  CHECK(msg.a == 11);

  msg.b = 123.0;
  CHECK(msg.b == 123.0);

  msg.c = -789;
  CHECK(msg.c == -789);

  // Bitfield is a little inconvenient, cannot set individually
  // using msg.d.a is impossible to emulate in C++, the only option is to assign all fields
  // together.
  using Bits = PlainMessage::Bits;
  msg.d = Bits{.a = 0, .b = 0x25};
  CHECK(Bits(msg.d).a == 0);
  CHECK(Bits(msg.d).b == 0x25);

  msg.e[1] = 0x1234;
  CHECK(msg.e[1] == 0x1234);

  msg.f[0] = 'a';
  CHECK(msg.f[0] == 'a');

  msg.g[1][1] = 0x12;
  CHECK(msg.g[1][1] == 0x12);

  auto g = std::span(msg.g);
  CHECK(g.size() == 2);

  auto h = FIELD_RANGE(msg, h);
  CHECK(h.size() == 0);

  // increase the size of msg, the span for h should increase
  msg.header.size = sizeof(PlainMessage) + 16;
  CHECK(msg.getMsgSize() == sizeof(PlainMessage) + 16);
  msg.h[0] = std::array<std::byte, 8>{};
  msg.h[0][1] = std::byte{0x12};
  constexpr std::byte seq[8] = {std::byte{0}, std::byte{1}, std::byte{2}, std::byte{3},
                                std::byte{4}, std::byte{5}, std::byte{6}, std::byte{7}};
  msg.h[1] = seq;

  h = FIELD_RANGE(msg, h);
  REQUIRE(h.size() == 2);
  for (const auto &hh : h) {
    CHECK(hh[0] == std::byte{0});
    CHECK(hh[1] != std::byte{0});
  }
  CHECK(h[0][1] == std::byte{0x12});
  CHECK(h[1][1] == std::byte{1});
  CHECK(h[1][7] == std::byte{7});

  pmsg->~PlainMessage();
}

struct WithUnion : MessageBase<WithUnion> {
  Field<uint16_t> size;
  Field<int> a;
  union {
    Field<int> b = {1};
    Field<double> c;
  };

  struct SubMsg {
    Field<int> a;
    Field<uint16_t> b;
  } d;
};

TEST_CASE("WithUnion Message") {
  WithUnion msg;
  msg.size = sizeof(WithUnion);
  CHECK(msg.getMsgSize() == sizeof(WithUnion));

  CHECK(msg.b == 1);
  msg.c = 123.0;
  CHECK(msg.c == 123.0);

  msg.d.a = 123;
  CHECK(msg.d.a == 123);
  msg.d.b = 456;
  CHECK(msg.d.b == 456);
}

struct BigEndianMessage : MessageBase<BigEndianMessage> {
  MessageHeader<std::endian::big> header{11, sizeof(BigEndianMessage)};
  Field<uint8_t, std::endian::big> a;
  Field<double, std::endian::big> b;
  Field<int, std::endian::big> c;

  struct Bits {
    uint8_t a : 1;
    uint8_t b : 7;
  };
  Field<Bits, std::endian::big> d;

  Field<uint16_t, std::endian::big> e[2];
  Field<char[8], std::endian::big> f;
  Field<unsigned char[8], std::endian::big> g[2];
};

TEST_CASE("BigEndian") {
  BigEndianMessage msg;

  CHECK(msg.header.msgid == 11);
  CHECK(msg.header.size == sizeof(BigEndianMessage));
  CHECK(msg.getMsgSize() == sizeof(BigEndianMessage));

  msg.a = 11;
  CHECK(msg.a == 11);

  msg.b = 123.0;
  CHECK(msg.b == 123.0);

  msg.c = 0x01020304;
  CHECK(msg.c == 0x01020304);
  CHECK(reinterpret_cast<unsigned char *>(&msg.c)[0] == 1);

  // Bitfield is a little inconvenient, cannot set individually
  using Bits = BigEndianMessage::Bits;
  msg.d = Bits{.a = 0, .b = 0x25};
  CHECK(Bits(msg.d).a == 0);
  CHECK(Bits(msg.d).b == 0x25);

  msg.e[1] = 0x1234;
  CHECK(msg.e[1] == 0x1234);

  msg.f[0] = 'a';
  CHECK(msg.f[0] == 'a');

  msg.g[1][1] = 0x12;
  CHECK(msg.g[1][1] == 0x12);

  auto g = std::span(msg.g);
  CHECK(g.size() == 2);
}
