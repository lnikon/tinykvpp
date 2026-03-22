#pragma once

#include <cstddef>
#include <cstdint>

namespace frankie::core {

struct arena_block {
  arena_block *next_{nullptr};

  char *data() noexcept { return reinterpret_cast<char *>(this) + sizeof(arena_block); }
};

class arena final {
 public:
  static constexpr std::uint64_t kDefaultBlockSize = static_cast<std::uint64_t>(4096) * 8;  // 32KB blocks
  static constexpr std::uint64_t kDefaultAlignment = 8;

  arena() = default;
  arena(const arena &) = delete;
  arena &operator=(const arena &) = delete;
  arena(arena &&) noexcept = default;
  arena &operator=(arena &&) noexcept = default;
  ~arena() noexcept = default;

  [[nodiscard]] static arena create(std::uint64_t capacity) noexcept;

  [[nodiscard]] void *allocate(std::uint64_t size, std::uint64_t align) noexcept;

  void destroy() noexcept;

  [[nodiscard]] std::uint64_t bytes_allocated() const noexcept;

 private:
  arena_block *current_ = nullptr;
  std::uint64_t block_size_ = kDefaultBlockSize;
  std::uint64_t offset_ = 0;
  std::uint64_t bytes_allocated_ = 0;
};

}  // namespace frankie::core
