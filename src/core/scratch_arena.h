#ifndef FRANKIE_SCRATCH_ARENA_H
#define FRANKIE_SCRATCH_ARENA_H

#include <cstdint>

namespace frankie::core {

class scratch_arena final {
 public:
  char *allocate(std::uint64_t size) noexcept;

  void reset() noexcept;

  ~scratch_arena();

 private:
  char *buf_ = nullptr;
  std::uint64_t offset_ = 0;
  std::uint64_t capacity_ = 0;
};

}  // namespace frankie::core

#endif  // FRANKIE_SCRATCH_ARENA_H
