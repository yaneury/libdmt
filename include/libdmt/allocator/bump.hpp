#pragma once

#include <array>
#include <cstdlib>
#include <libdmt/allocator/parameters.hpp>
#include <libdmt/internal/chunk.hpp>
#include <libdmt/internal/platform.hpp>
#include <libdmt/internal/types.hpp>
#include <libdmt/internal/util.hpp>

namespace dmt::allocator {

// TODO: Add synchronization support.
template <class T, typename... Args> class Bump {
public:
  // Require alias for std::allocator_traits to infer other types, e.g.
  // using pointer = value_type*.
  using value_type = T;

  explicit Bump(){};

  ~Bump() { Reset(); }

  template <class U> constexpr Bump(const Bump<U>&) noexcept {}

  T* allocate(std::size_t n) noexcept {
    if (n > AlignedSize_)
      return nullptr;

    if (!chunks_) {
      if (chunks_ = AllocateNewChunk(); !chunks_)
        return nullptr;

      // Set current chunk to header
      current_ = chunks_;
    }

    size_t request_size = internal::AlignUp(n, Alignment_);
    size_t remaining_size =
        AlignedSize_ - offset_ - dmt::internal::GetChunkHeaderSize();

    if (request_size > remaining_size) {
      if (!GrowWhenFull_)
        return nullptr;

      auto* chunk = AllocateNewChunk();
      if (!chunk)
        return nullptr;

      current_->next = chunk;
      current_ = chunk;
      offset_ = 0;
    }

    internal::Byte* base = dmt::internal::GetChunk(current_);
    internal::Byte* result = base + offset_;
    offset_ += request_size;

    return reinterpret_cast<T*>(result);
  }

  void deallocate(T*, std::size_t) noexcept {
    // The bump allocator does not support per-object deallocation.
  }

  void Reset() {
    offset_ = 0;
    if (chunks_)
      ReleaseChunks(chunks_);
    chunks_ = nullptr;
  }

private:
  // There are several factors used to determine the alignment for the
  // allocator. First, users can specify their own alignment if desired using
  // |AlignmentT<>|. Otherwise, we use the alignment as determined by the C++
  // compiler. There's a floor in the size of the alignment to be equal to or
  // greater than |sizeof(void*)| for compatibility with std::aligned_alloc.
  static constexpr std::size_t Alignment_ =
      std::max({std::alignment_of_v<T>, sizeof(void*),
                internal::GetValueT<AlignmentT<0>, Args...>::value});

  static_assert(internal::IsPowerOfTwo(Alignment_),
                "Alignment must be a power of 2.");

  static constexpr std::size_t RequestSize_ =
      internal::GetValueT<SizeT<kDefaultSize>, Args...>::value;

  static constexpr std::size_t AlignedSize_ = internal::AlignUp(
      RequestSize_ + internal::GetChunkHeaderSize(), Alignment_);

  static constexpr bool GrowWhenFull_ =
      internal::GetValueT<GrowT<WhenFull::GrowStorage>, Args...>::value ==
      WhenFull::GrowStorage;

  static internal::Allocation CreateAllocation(internal::Byte* base) {
    std::size_t size = IsPageMultiple() ? AlignedSize_ / internal::GetPageSize()
                                        : AlignedSize_;
    return internal::Allocation{.base = static_cast<internal::Byte*>(base),
                                .size = size};
  }

  static bool IsPageMultiple() {
    static const auto page_size = internal::GetPageSize();
    return AlignedSize_ > page_size && AlignedSize_ % page_size == 0;
  }

  static dmt::internal::ChunkHeader* AllocateNewChunk() {
    auto allocation =
        IsPageMultiple()
            ? internal::AllocatePages(AlignedSize_ / internal::GetPageSize())
            : internal::AllocateBytes(AlignedSize_, Alignment_);

    if (!allocation.has_value())
      return nullptr;

    return dmt::internal::CreateChunkHeaderFromAllocation(allocation.value());
  }

  static void ReleaseChunks(dmt::internal::ChunkHeader* chunk) {
    auto release =
        IsPageMultiple() ? internal::ReleasePages : internal::ReleaseBytes;
    dmt::internal::ReleaseChunks(chunk, std::move(release));
  }

  size_t offset_ = 0;
  dmt::internal::ChunkHeader* chunks_ = nullptr;
  dmt::internal::ChunkHeader* current_ = nullptr;
};

template <class T, class U> bool operator==(const Bump<T>&, const Bump<U>&) {
  return true;
}

template <class T, class U> bool operator!=(const Bump<T>&, const Bump<U>&) {
  return false;
}

} // namespace dmt::allocator