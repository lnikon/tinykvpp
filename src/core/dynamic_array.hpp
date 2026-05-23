#pragma once

#include <expected>

#include "core/arena.hpp"
#include "core/assert.hpp"
#include "core/status.hpp"

namespace frankie::core {

template <typename TEntry>
class dynamic_array final {
  // Arena backing the underlying memory of the array.
  core::arena *arena_{nullptr};
  // Memory image of the array.
  TEntry *items_{nullptr};
  // Capacity of the memory image.
  std::uint32_t capacity_{0};
  // Number of entries in memory image.
  std::uint32_t size_{0};

 public:
  dynamic_array() = default;
  dynamic_array(const dynamic_array &) = delete;
  dynamic_array &operator=(const dynamic_array &) = delete;
  dynamic_array(dynamic_array &&) = default;
  dynamic_array &operator=(dynamic_array &&) = default;
  ~dynamic_array() {
    if (items_ != nullptr) {
      arena_->destroy();
      items_ = nullptr;
      capacity_ = 0;
      size_ = 0;
    }
  }

  [[nodiscard]] static std::expected<dynamic_array<TEntry>, core::status> create(core::arena *arena,
                                                                                 std::uint32_t capacity) noexcept {
    FR_VERIFY_MSG(capacity > 0ULL, "capacity should be greater than zero");

    auto *buffer = arena->allocate(capacity, alignof(TEntry));
    if (buffer == nullptr) {
      return core::unexpected(core::status_code::out_of_memory);
    }

    dynamic_array<TEntry> result;
    result.arena_ = arena;
    result.items_ = static_cast<TEntry *>(buffer);
    result.capacity_ = capacity;
    result.size_ = 0;
    return result;
  }

  void append(TEntry &&entry) noexcept {
    if (size_ >= capacity_) {
      if (auto res = increase_capacity(capacity_ * 2); !res) {
        return core::unexpected(res.error());
      }
    }

    items_[size_++] = std::move(entry);
  }

  [[nodiscard]] std::uint32_t size() noexcept { return size_; }

  [[nodiscard]] std::uint32_t capacity() noexcept { return capacity_; }

  [[nodiscard]] bool empty() noexcept { return size_ == 0; }

 private:
  std::expected<void, core::status> increase_capacity(std::uint32_t new_capacity) noexcept {
    FR_VERIFY_MSG(arena_ != nullptr, "arena should not be null");
    FR_VERIFY_MSG(new_capacity > 0ULL, "capacity must be greater than zero");
    FR_VERIFY_MSG(new_capacity > capacity_, "capacity must be greater than existing capacity");
    FR_VERIFY_MSG(items_ != nullptr, "items should be not-null");

    // TODO(lnikon): No way to clear already allocated array:
    //               (1) Either need to introduce new arena based on freelist,
    //               (2) or modify arena interface to support reallocation, but what if that arena is shared between
    //               different objects?
    //
    //               A workaround would be to manually execute free() on items_, after successfully allocating new
    //               array.
    auto *buffer = arena_->allocate(new_capacity, alignof(TEntry));
    if (buffer == nullptr) {
      return core::unexpected(core::status_code::out_of_memory);
    }

    auto *new_items = static_cast<TEntry>(buffer);
    std::memcpy(new_items, items_, size_);

    // TODO(lnikon): This free() call is a manual workaround until a proper arena is introduced.
    std::free(items_);

    items_ = new_items;
    capacity_ = new_capacity;

    return {};
  }
};

}  // namespace frankie::core
