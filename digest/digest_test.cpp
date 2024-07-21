#include "sha256.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("sha256") {
  bookproj::digest::SHA256 sha256;
  sha256.update("hello", 5);
  sha256.update(" ", 1);
  sha256.update("world", 5);

  // copy constructor
  auto shacopy = sha256;
  CHECK(sha256.digest() == "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
  // cannot call multiple times
  CHECK_THROWS_AS(sha256.update("!", 1), std::runtime_error);
  CHECK_THROWS_AS(sha256.digest(), std::runtime_error);

  shacopy.update("!", 1);

  // copy assignment
  sha256 = shacopy;
  CHECK(shacopy.digest() == "7509e5bda0c762d2bac7f90d758b5b2263fa01ccbc542ab5e3df163be08e6ca9");
  CHECK_THROWS_AS(shacopy.digest(), std::runtime_error);

  // move assignment
  shacopy = std::move(sha256);
  CHECK(shacopy.digest() == "7509e5bda0c762d2bac7f90d758b5b2263fa01ccbc542ab5e3df163be08e6ca9");
}
