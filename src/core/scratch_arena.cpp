#include "scratch_arena.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>
#include "core/assert.hpp"

namespace frankie::core {

scratch_arena::scratch_arena(scratch_arena &&other) noexcept
    : buf_{std::exchange(other.buf_, nullptr)},
      offset_{std::exchange(other.offset_, 0)},
      capacity_{std::exchange(other.capacity_, 0)} {}

scratch_arena &scratch_arena::operator=(scratch_arena &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  destroy();

  buf_ = std::exchange(other.buf_, nullptr);
  offset_ = std::exchange(other.offset_, 0);
  capacity_ = std::exchange(other.capacity_, 0);

  return *this;
}

scratch_arena::~scratch_arena() noexcept { destroy(); }

char *scratch_arena::allocate(std::uint64_t size) noexcept {
  if (offset_ + size > capacity_) {
    capacity_ = std::max(capacity_ * 2, offset_ + size);
    // TODO(lnikon): realloc return NULL on error, add a handler here.
    buf_ = static_cast<char *>(std::realloc(buf_, capacity_));
  }
  char *ptr = buf_ + offset_;
  offset_ += size;
  return ptr;
}

void scratch_arena::reset() noexcept { offset_ = 0; }

// bool scratch_arena::realloc(std::uint64_t new_capacity) noexcept {
//   FR_VERIFY(new_capacity > capacity_);
//
//   char *new_buf = ::malloc(new_capacity);
//   if (new_buf == nullptr) {
//     return false;
//   }
//
//   if (buf_ != nullptr) {
//     std::memcpy(new_buf, buf_, capacity_);
//   }
// }

void scratch_arena::destroy() noexcept {
  if (buf_ != nullptr) {
    std::free(buf_);
    buf_ = nullptr;
    offset_ = 0;
    capacity_ = 0;
  }
}

}  // namespace frankie::core
