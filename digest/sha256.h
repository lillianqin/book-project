#pragma once

#include <openssl/evp.h>
#include <string>
#include <utility>

namespace bookproj {
namespace digest {
struct SHA256 {

  SHA256();
  SHA256(const SHA256 &);
  SHA256(SHA256 &&other) noexcept : ctx(other.ctx) { other.ctx = nullptr; }

  SHA256 &operator=(const SHA256 &other);
  SHA256 &operator=(SHA256 &&other) noexcept {
    std::swap(ctx, other.ctx);
    return *this;
  }

  ~SHA256();

  // throws std::runtime_error if finalDigest() is already called
  void update(const void *data, size_t size);

  // once called, can no longer call update or digest.  throws std::runtime_errorif called
  // multiple times
  std::string digest();

private:
  void reset();
  EVP_MD_CTX *ctx;
};

} // namespace digest
} // namespace bookproj