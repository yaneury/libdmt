#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <ranges>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <dmt/allocator/error.hpp>
#include <dmt/allocator/internal/block.hpp>

using namespace dmt::allocator;
using namespace dmt::allocator::internal;

template <class T> inline T* FromBytePtr(std::byte* p) {
  return reinterpret_cast<T*>(p);
}

template <class T> inline std::byte* ToBytePtr(T* p) {
  return reinterpret_cast<std::byte*>(p);
}

template <class T> inline T GetValueOrFail(Result<T> result) {
  REQUIRE(result.has_value());
  return result.value();
}

template <class T> inline T* GetPtrOrFail(Result<std::byte*> result) {
  return FromBytePtr<T>(GetValueOrFail(result));
}

inline constexpr std::size_t SizeWithHeader(std::size_t sz) {
  return sz + GetBlockHeaderSize();
}

class TestFreeList {
public:
  static TestFreeList FromBlockSizes(std::vector<std::size_t> block_sizes) {
    std::size_t total_size = 0;
    std::ranges::for_each(block_sizes, [&total_size](std::size_t& sz) {
      sz += GetBlockHeaderSize();
      total_size += sz;
    });

    auto buffer = std::make_unique<std::byte[]>(total_size);

    std::byte* itr = buffer.get();
    for (std::size_t i = 0; i < block_sizes.size(); ++i) {
      auto size = block_sizes[i];
      BlockHeader* h = reinterpret_cast<BlockHeader*>(itr);
      h->size = size;
      itr = itr + size;
      if (i < block_sizes.size() - 1)
        h->next = reinterpret_cast<BlockHeader*>(itr);
    }

    return TestFreeList(std::move(buffer), std::move(block_sizes));
  }

  BlockHeader* AsHeader() {
    CHECK(buffer_ != nullptr);
    return reinterpret_cast<BlockHeader*>(buffer_.get());
  }

  BlockHeader* GetHeader(std::size_t target) {
    CHECK(target < block_sizes_.size());

    std::size_t offset =
        std::accumulate(begin(block_sizes_), begin(block_sizes_) + target, 0);

    return reinterpret_cast<BlockHeader*>(buffer_.get() + offset);
  }

private:
  TestFreeList() = delete;

  TestFreeList(std::unique_ptr<std::byte[]> buffer,
               std::vector<std::size_t> block_sizes)
      : buffer_(std::move(buffer)), block_sizes_(std::move(block_sizes)) {}

  std::vector<std::size_t> block_sizes_;
  std::unique_ptr<std::byte[]> buffer_;
};
