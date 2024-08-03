#pragma once
#include <absl/log/log.h>
#include <cstddef>
#include <vector>

namespace bookproj {
template <typename T> class ObjectPool {
  union S {
    alignas(T) std::byte storage[sizeof(T)];
    S *next;
  };

  // allocation unit in bytes
  static constexpr size_t DefaultChunkSize = 2ul << 20;
  static constexpr size_t ObjSize = sizeof(S);

public:
  ObjectPool() noexcept
      : ObjectPool(DefaultChunkSize < ObjSize ? 1 : DefaultChunkSize / ObjSize) {}
  // batchSize is objects to allocate in one go
  explicit ObjectPool(size_t batchSize) noexcept : chunkByteSize(batchSize * ObjSize) {}

  ObjectPool(const ObjectPool &) = delete;
  ObjectPool &operator=(const ObjectPool &) = delete;

  ~ObjectPool() {
    if (numAllocated() != 0) {
      LOG(ERROR) << numAllocated() << " objects are not destroyed at destruction of ObjectPool";
    }
    for (auto c : chunks) {
      delete[] c;
    }
  }

  template <typename... Args> T *create(Args &&...args) {
    if (0 == freeCount) [[unlikely]] {
      reserve(chunkByteSize / ObjSize);
    }
    S *s = freeList;
    freeList = freeList->next;
    --freeCount;
    return new (s->storage) T(std::forward<Args>(args)...);
  }

  void destroy(T *t) {
    t->~T();
    S *s = reinterpret_cast<S *>(t);
    s->next = freeList;
    freeList = s;
    ++freeCount;
  }

  void reserve(size_t nobjs) {
    while (freeCount < nobjs) {
      S *ss = new S[chunkByteSize / ObjSize];
      for (size_t i = 0; i < chunkByteSize / ObjSize; ++i) {
        S *s = ss + (chunkByteSize / ObjSize - i - 1);
        s->next = freeList;
        freeList = s;
        ++freeCount;
      }
      chunks.push_back(ss);
    }
  }

  size_t numFree() const { return freeCount; }
  size_t numAllocated() const { return chunks.size() * (chunkByteSize / ObjSize) - freeCount; }

private:
  const size_t chunkByteSize;

  S *freeList = nullptr;
  std::vector<S *> chunks;
  size_t freeCount = 0;
};
} // namespace bookproj