#ifndef FRANKIE_SCRATCH_ARENA_H
#define FRANKIE_SCRATCH_ARENA_H

#include <cstdint>

namespace frankie::core {

class scratch_arena final {
 public:
  scratch_arena() = default;
  scratch_arena(const scratch_arena &) = delete;
  scratch_arena &operator=(const scratch_arena &) = delete;
  scratch_arena(scratch_arena &&) noexcept = default;
  scratch_arena &operator=(scratch_arena &&) noexcept = default;
  ~scratch_arena() noexcept = default;

  [[nodiscard]] char *allocate(std::uint64_t size) noexcept;

  void reset() noexcept;

  void destroy() noexcept;

 private:
  char *buf_ = nullptr;
  std::uint64_t offset_ = 0;
  std::uint64_t capacity_ = 0;
};

}  // namespace frankie::core

#endif  // FRANKIE_SCRATCH_ARENA_H
