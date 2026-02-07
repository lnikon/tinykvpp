#pragma once

#include <cstdint>
#include <random>

#include "core/arena.hpp"

namespace frankie::storage {

constexpr const std::uint32_t DEFAULT_MAX_HEIGHT = 12;
constexpr const std::uint32_t DEFAULT_BRANCHING_FACTOR = 4;

///
/// Memory layout:
/// [key_size_|value_size_|height_|pad|fwd[0]..fwd[h-1]|key|value]
///
struct alignas(void*) skiplist_node {
  std::uint32_t key_size_;
  std::uint32_t value_size_;
  std::uint32_t height_;

  skiplist_node** forward() noexcept {
    return reinterpret_cast<skiplist_node**>(reinterpret_cast<char*>(this) +
                                             sizeof(skiplist_node));
  }

  std::string_view key() noexcept {
    char* base = reinterpret_cast<char*>(forward() + height_);
    return {base, key_size_};
  }

  std::string_view value() noexcept {
    char* base = reinterpret_cast<char*>(forward() + height_) + key_size_;
    return {base, value_size_};
  }
};
static_assert(alignof(skiplist_node) >= alignof(skiplist_node*));

///
/// Stores (key, value) pairs. Ordered by key.
/// Randomized balancing.
/// Supports insertion, search, and iteration.
/// Does not support deletion of a single node.
/// Deallocated all at once.
/// Requires arena allocator.
/// Does not support update of existing key.
///
struct skiplist {
  core::arena* arena_;

  skiplist_node* head_;

  std::uint32_t max_height_;
  std::uint32_t current_height_;
  std::uint32_t branching_factor_;
  std::uint32_t count_{0};

  std::mt19937 rng_{std::random_device{}()};
};

struct skiplist_iter {
  skiplist_node* current_;
};

std::uint32_t random_height(skiplist* sl) noexcept;

skiplist_node* create_skiplist_node(core::arena* arena, std::string_view key,
                                    std::string_view value,
                                    std::uint32_t height) noexcept;

skiplist* create_skiplist(core::arena* arena, std::uint32_t max_height,
                          std::uint32_t branching_factor) noexcept;

skiplist_node* skiplist_search(skiplist* sl, std::string_view key) noexcept;

skiplist_node* skiplist_insert(skiplist* sl, std::string_view key,
                               std::string_view value) noexcept;

skiplist_iter skiplist_seek(skiplist* sl, std::string_view key) noexcept;

bool skiplist_empty(skiplist* sl) noexcept;

std::uint32_t skiplist_count(skiplist* sl) noexcept;

skiplist_iter create_skiplist_iter(skiplist_node* node) noexcept;

bool skiplist_iter_valid(skiplist_iter* sl) noexcept;

void skiplist_iter_next(skiplist_iter* sl) noexcept;

std::string_view skiplist_iter_key(skiplist_iter* sl) noexcept;

std::string_view skiplist_iter_value(skiplist_iter* sl) noexcept;

}  // namespace frankie::storage
