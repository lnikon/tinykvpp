#pragma once

#include <cstdint>

namespace frankie::core {

struct arena_block {
  arena_block* next_;

  char* data() noexcept {
    return reinterpret_cast<char*>(this) + sizeof(arena_block);
  }
};

struct arena {
  static constexpr std::uint64_t kDefaultBlockSize = 4096 * 8;  // 32KB blocks

  arena_block* current_ = nullptr;
  std::uint64_t block_size_ = kDefaultBlockSize;
  std::uint64_t offset_ = 0;
  std::uint64_t bytes_allocated_ = 0;
};

void* arena_allocate(arena* a, std::uint64_t size,
                     std::uint64_t align) noexcept;

void arena_destroy(arena* a) noexcept;

}  // namespace frankie::core
