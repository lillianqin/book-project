#include "sha256.h"
#include <stdexcept>

namespace bookproj {
namespace digest {

SHA256::SHA256() : ctx(EVP_MD_CTX_new()) { EVP_DigestInit_ex2(ctx, EVP_sha256(), nullptr); }
SHA256::~SHA256() { reset(); }

SHA256::SHA256(const SHA256 &other) : ctx(EVP_MD_CTX_new()) { EVP_MD_CTX_copy_ex(ctx, other.ctx); }

SHA256 &SHA256::operator=(const SHA256 &other) {
  if (this != &other) {
    reset();
    ctx = EVP_MD_CTX_new();
    EVP_MD_CTX_copy_ex(ctx, other.ctx);
  }
  return *this;
}

void SHA256::update(const void *data, size_t size) {
  if (nullptr == ctx) {
    throw std::runtime_error("Cannot call update after retrieving digest");
  }
  EVP_DigestUpdate(ctx, data, size);
}

std::string SHA256::digest() {
  if (nullptr == ctx) {
    throw std::runtime_error("Cannot call digest after retrieving digest");
  }
  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;
  EVP_DigestFinal_ex(ctx, md_value, &md_len);
  reset();
  std::string result(md_len * 2, ' ');
  static constexpr char hex[] = "0123456789abcdef";
  for (unsigned int ii = 0; ii < md_len; ii++) {
    result[ii * 2] = hex[(md_value[ii] >> 4) & 0xf];
    result[ii * 2 + 1] = hex[md_value[ii] & 0xf];
  }
  return result;
}

void SHA256::reset() {
  if (nullptr != ctx) {
    EVP_MD_CTX_free(ctx);
    ctx = nullptr;
  }
}

} // namespace digest
} // namespace bookproj