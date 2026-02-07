#include "core/arena.hpp"

#include <algorithm>

namespace frankie::core {

void* arena_allocate(arena* a, std::uint64_t size,
                     std::uint64_t align) noexcept {
  a->offset_ = (a->offset_ + align - 1) & ~(align - 1);

  if (a->current_ == nullptr || a->offset_ + size > a->block_size_) {
    std::uint64_t capacity = std::max(a->block_size_, size);
    auto* block =
        static_cast<arena_block*>(std::malloc(sizeof(arena_block) + capacity));
    block->next_ = a->current_;
    a->current_ = block;
    a->offset_ = 0;
  }

  void* ptr = a->current_->data() + a->offset_;
  a->offset_ += size;
  a->bytes_allocated_ += size;

  return ptr;
}

void arena_destroy(arena* a) noexcept {
  arena_block* block = a->current_;
  while (block != nullptr) {
    arena_block* prev = block->next_;
    std::free(block);
    block = prev;
  }
  a->current_ = nullptr;
  a->offset_ = 0;
  a->bytes_allocated_ = 0;
}

}  // namespace frankie::core
