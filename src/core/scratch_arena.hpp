#ifndef FRANKIE_SCRATCH_ARENA_H
#define FRANKIE_SCRATCH_ARENA_H

#include <cstdint>

namespace frankie::core {

constexpr inline std::uint64_t kScratchAlignment = 64;

class scratch_arena final {
 public:
  scratch_arena() = default;
  scratch_arena(const scratch_arena &) = delete;
  scratch_arena &operator=(const scratch_arena &) = delete;
  scratch_arena(scratch_arena &&) noexcept;
  scratch_arena &operator=(scratch_arena &&) noexcept;
  ~scratch_arena() noexcept;

  [[nodiscard]] char *allocate(std::uint64_t size) noexcept;

  void reset() noexcept;

  // TODO(lnikon): Needs rethinking...
  // [[nodiscard]] bool realloc(std::uint64_t new_capacity) noexcept;

  void destroy() noexcept;

 private:
  char *buf_ = nullptr;
  std::uint64_t offset_ = 0;
  std::uint64_t capacity_ = 0;
};

}  // namespace frankie::core

#endif  // FRANKIE_SCRATCH_ARENA_H
