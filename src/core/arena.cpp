#include "core/arena.hpp"

#include <algorithm>

namespace frankie::core {

arena arena::create(const std::uint64_t capacity) noexcept {
  arena result;
  result.current_ = static_cast<arena_block *>(std::malloc(sizeof(arena_block) + capacity));
  result.current_->next_ = nullptr;
  result.block_size_ = capacity;
  result.offset_ = 0;
  result.bytes_allocated_ = sizeof(arena_block);
  return result;
}

void *arena::allocate(const std::uint64_t size, std::uint64_t align) noexcept {
  offset_ = (offset_ + align - 1) & ~(align - 1);

  if (current_ == nullptr || offset_ + size > block_size_) {
    const std::uint64_t capacity = std::max(block_size_, size);
    auto *block = static_cast<arena_block *>(std::malloc(sizeof(arena_block) + capacity));
    block->next_ = current_;
    current_ = block;
    offset_ = 0;
    bytes_allocated_ += sizeof(arena_block);
  }

  void *ptr = current_->data() + offset_;
  offset_ += size;
  bytes_allocated_ += size;

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
