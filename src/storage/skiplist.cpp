#include "storage/skiplist.hpp"

#include <cassert>
#include <cstring>

namespace frankie::storage {

std::uint32_t random_height(skiplist* sl) noexcept {
  std::uint32_t height = 1;
  std::uniform_int_distribution<std::uint32_t> dist(0,
                                                    sl->branching_factor_ - 1);
  while (height < sl->max_height_ && dist(sl->rng_) == 0) {
    height++;
  }
  return height;
}

skiplist_node* create_skiplist_node(core::arena* arena, std::string_view key,
                                    std::string_view value,
                                    std::uint32_t height) noexcept {
  const std::uint64_t size = sizeof(skiplist_node) +
                             height * sizeof(skiplist_node*) + key.size() +
                             value.size();

  void* buffer = core::arena_allocate(arena, size, alignof(skiplist_node));
  std::memset(buffer, 0, size);

  auto* node = static_cast<skiplist_node*>(buffer);
  node->key_size_ = static_cast<std::uint32_t>(key.size());
  node->value_size_ = static_cast<std::uint32_t>(value.size());
  node->height_ = height;

  std::memcpy(node->forward() + height, key.data(), key.size());
  std::memcpy(reinterpret_cast<char*>(node->forward() + height) + key.size(),
              value.data(), value.size());

  return node;
}

skiplist* create_skiplist(core::arena* arena, std::uint32_t max_height,
                          std::uint32_t branching_factor) noexcept {
  skiplist* sl = new struct skiplist;

  sl->arena_ = arena;
  sl->head_ = create_skiplist_node(sl->arena_, {}, {}, max_height);
  sl->max_height_ = max_height;
  sl->current_height_ = 0;
  sl->branching_factor_ = branching_factor;

  return sl;
}

skiplist_node* skiplist_search(skiplist* sl, std::string_view key) noexcept {
  skiplist_node* current_node = sl->head_;
  for (std::int32_t current_level =
           static_cast<std::int32_t>(sl->max_height_ - 1);
       current_level >= 0; current_level--) {
    const std::uint64_t current_level_u64 =
        static_cast<std::uint64_t>(current_level);
    while (current_node->forward()[current_level_u64] != nullptr &&
           current_node->forward()[current_level_u64]->key() < key) {
      current_node = current_node->forward()[current_level_u64];
    }
  }
  current_node = current_node->forward()[0];
  if (current_node != nullptr && current_node->key() == key) {
    return current_node;
  }
  return nullptr;
}

skiplist_node* skiplist_insert(skiplist* sl, std::string_view key,
                               std::string_view value) noexcept {
  skiplist_node* updates[DEFAULT_MAX_HEIGHT];

  skiplist_node* current_node = sl->head_;
  for (std::int32_t current_level =
           static_cast<std::int32_t>(sl->max_height_ - 1);
       current_level >= 0; current_level--) {
    const std::uint64_t current_level_u64 =
        static_cast<std::uint64_t>(current_level);

    while (current_node->forward()[current_level_u64] != nullptr &&
           current_node->forward()[current_level_u64]->key() < key) {
      current_node = current_node->forward()[current_level_u64];
    }
    updates[current_level_u64] = current_node;
  }

  if (current_node->forward()[0] != nullptr &&
      current_node->forward()[0]->key() == key) {
    return current_node->forward()[0];
  }

  const std::uint32_t new_height = random_height(sl);
  if (new_height > sl->current_height_) {
    for (std::uint32_t level = sl->current_height_; level < new_height;
         level++) {
      updates[level] = sl->head_;
    }
    sl->current_height_ = new_height;
  }

  skiplist_node* new_node =
      create_skiplist_node(sl->arena_, key, value, new_height);
  for (std::uint32_t level = 0; level < new_height; level++) {
    new_node->forward()[level] = updates[level]->forward()[level];
    updates[level]->forward()[level] = new_node;
  }

  sl->count_++;

  return new_node;
}

skiplist_iter skiplist_seek(skiplist* sl, std::string_view key) noexcept {
  skiplist_node* current_node = sl->head_;
  for (std::int32_t current_level =
           static_cast<std::int32_t>(sl->max_height_ - 1);
       current_level >= 0; current_level--) {
    const std::uint64_t current_level_u64 =
        static_cast<std::uint64_t>(current_level);
    while (current_node->forward()[current_level_u64] != nullptr &&
           current_node->forward()[current_level_u64]->key() < key) {
      current_node = current_node->forward()[current_level_u64];
    }
  }
  return create_skiplist_iter(current_node->forward()[0]);
}

bool skiplist_empty(skiplist* sl) noexcept { return sl->count_ == 0; }

std::uint32_t skiplist_count(skiplist* sl) noexcept { return sl->count_; }

skiplist_iter create_skiplist_iter(skiplist_node* node) noexcept {
  return skiplist_iter{.current_ = node};
}

bool skiplist_iter_valid(skiplist_iter* sl) noexcept {
  return sl->current_ != nullptr;
}

void skiplist_iter_next(skiplist_iter* sl) noexcept {
  assert(sl->current_ != nullptr);
  if (sl->current_ != nullptr) {
    sl->current_ = sl->current_->forward()[0];
  }
}

std::string_view skiplist_iter_key(skiplist_iter* sl) noexcept {
  assert(sl->current_ != nullptr);
  return sl->current_->key();
}

std::string_view skiplist_iter_value(skiplist_iter* sl) noexcept {
  assert(sl->current_ != nullptr);
  return sl->current_->value();
}

}  // namespace frankie::storage
