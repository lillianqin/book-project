#include "itch50.h"
#include <catch2/catch_test_macros.hpp>

using namespace bookproj;

TEST_CASE("basic") {
  itch50::SystemEvent se;
  CHECK(se.header.messageType == 'S');
}
