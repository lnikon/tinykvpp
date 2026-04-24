#pragma once

#include <cstdint>

namespace frankie::core {

// Used to align arena_block & perform aligned_alloc.
inline constexpr std::uint64_t kBlockAlignment = 64;
// Default block size to use when user provided size is less than 32KB.
inline constexpr std::uint64_t kDefaultBlockSize = static_cast<std::uint64_t>(4096) * 8;  // 32KB blocks

// Cache aligned arena block.
struct alignas(kBlockAlignment) arena_block {
  arena_block *next_{nullptr};

  char *data() noexcept { return reinterpret_cast<char *>(this) + sizeof(arena_block); }
};

namespace detail {

// Return (sizeof(arena_block) + capacity) rounded up to a multiple of kBlockAlignment.
[[nodiscard]] std::uint64_t block_capacity_rounded(std::uint64_t capacity) noexcept;

// Return block allocated with kBlockAlignment aligned
[[nodiscard]] arena_block *fixed_aligned_alloc(std::uint64_t capacity) noexcept;

}  // namespace detail

class arena final {
 public:
  arena() = default;
  arena(const arena &) = delete;
  arena &operator=(const arena &) = delete;
  arena(arena &&) noexcept;
  arena &operator=(arena &&) noexcept;
  ~arena() noexcept = default;

  [[nodiscard]] static arena create(std::uint64_t capacity) noexcept;

  [[nodiscard]] void *allocate(std::uint64_t requested_capacity, std::uint64_t align) noexcept;

  void destroy() noexcept;

  [[nodiscard]] std::uint64_t bytes_allocated() const noexcept;

 private:
  // Pointer to currently active block.
  arena_block *current_ = nullptr;
  // Default capacity of a block (excluding metadata).
  std::uint64_t default_block_size_ = kDefaultBlockSize;
  // Capacity of the current block (excluding metadata).
  std::uint64_t current_block_size_{0};
  // Offset (number of bytes used) within currently active block.
  std::uint64_t offset_ = 0;
  // Total bytes allocated for all blocks including the metadata.
  std::uint64_t bytes_allocated_ = 0;
};

}  // namespace frankie::core
