#include <algorithm>
#include <utility>

#include "core/arena.hpp"
#include "core/assert.hpp"

namespace frankie::core {

namespace detail {

std::uint64_t block_capacity_rounded(std::uint64_t capacity) noexcept {
  return (sizeof(arena_block) + capacity + kBlockAlignment - 1) & ~(kBlockAlignment - 1);
}

arena_block *fixed_aligned_alloc(std::uint64_t capacity) noexcept {
  return static_cast<arena_block *>(std::aligned_alloc(kBlockAlignment, block_capacity_rounded(capacity)));
}

}  // namespace detail

arena::arena(arena &&other) noexcept
    : current_{std::exchange(other.current_, nullptr)},
      default_block_size_{std::exchange(other.default_block_size_, 0)},
      current_block_size_{std::exchange(other.current_block_size_, 0)},
      offset_{std::exchange(other.offset_, 0)},
      bytes_allocated_{std::exchange(other.bytes_allocated_, 0)} {}

arena &arena::operator=(arena &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  destroy();

  current_ = std::exchange(other.current_, nullptr);
  default_block_size_ = std::exchange(other.default_block_size_, 0);
  current_block_size_ = std::exchange(other.current_block_size_, 0);
  offset_ = std::exchange(other.offset_, 0);
  bytes_allocated_ = std::exchange(other.bytes_allocated_, 0);

  return *this;
}

arena arena::create(const std::uint64_t capacity) noexcept {
  arena result;
  result.current_ = detail::fixed_aligned_alloc(capacity);
  result.current_->next_ = nullptr;
  result.default_block_size_ = capacity;
  result.current_block_size_ = capacity;
  result.offset_ = 0;
  result.bytes_allocated_ = detail::block_capacity_rounded(capacity);
  return result;
}

void *arena::allocate(const std::uint64_t requested_capacity, const std::uint64_t alignment) noexcept {
  FR_DEBUG_ASSERT(alignment != 0ULL);
  FR_DEBUG_ASSERT((alignment & (alignment - 1)) == 0ULL);  // power of 2
  FR_DEBUG_ASSERT(alignment <= kBlockAlignment);           // block only guarantees 64

  offset_ = (offset_ + alignment - 1) & ~(alignment - 1);
  if (current_ == nullptr || offset_ + requested_capacity > current_block_size_) {
    // Update memory block
    const std::uint64_t actual_capacity = std::max(default_block_size_, requested_capacity);
    auto *block = detail::fixed_aligned_alloc(actual_capacity);
    block->next_ = current_;
    current_ = block;

    // Update metadata of the current block
    current_block_size_ = actual_capacity;
    offset_ = 0;

    // Update number of bytes used for ALL blocks including metadata
    bytes_allocated_ += detail::block_capacity_rounded(actual_capacity);
  }

  void *ptr = current_->data() + offset_;
  offset_ += requested_capacity;

  return ptr;
}

void arena::destroy() noexcept {
  arena_block *block = current_;
  while (block != nullptr) {
    arena_block *prev = block->next_;
    std::free(block);
    block = prev;
  }
  current_ = nullptr;
  offset_ = 0;
  bytes_allocated_ = 0;
}

std::uint64_t arena::bytes_allocated() const noexcept { return bytes_allocated_; }

}  // namespace frankie::core
