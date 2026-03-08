#include "scratch_arena.h"

#include <algorithm>
#include <cstdlib>

namespace frankie::core {

char *scratch_arena::allocate(std::uint64_t size) noexcept {
  if (offset_ + size > capacity_) {
    capacity_ = std::max(capacity_ * 2, offset_ + size);
    buf_ = static_cast<char *>(std::realloc(buf_, capacity_));
  }
  char *ptr = buf_ + offset_;
  offset_ += size;
  return ptr;
}

void scratch_arena::reset() noexcept { offset_ = 0; }

scratch_arena::~scratch_arena() { std::free(buf_); }

}  // namespace frankie::core