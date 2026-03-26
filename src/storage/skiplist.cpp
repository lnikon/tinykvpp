#include "storage/skiplist.hpp"

namespace frankie::storage {

skiplist_node *skiplist_node::create(core::arena *a, std::string_view key, std::string_view value,
                                     std::uint32_t height) noexcept {
  const auto size = layout_size(height, key.size(), value.size());
  void *mem = a->allocate(static_cast<std::uint64_t>(size), alignof(skiplist_node));
  std::memset(mem, 0, size);

  auto *node = ::new (mem) skiplist_node{};
  node->key_size_ = static_cast<std::uint32_t>(key.size());
  node->value_size_ = static_cast<std::uint32_t>(value.size());
  node->height_ = height;

  std::memcpy(node->key_bytes().data(), key.data(), key.size());
  std::memcpy(node->value_bytes().data(), value.data(), value.size());

  return node;
}

std::span<skiplist_node *> skiplist_node::forward() noexcept {
  auto *base = reinterpret_cast<skiplist_node **>(reinterpret_cast<std::byte *>(this) + sizeof(skiplist_node));
  return {base, height_};
}

std::span<const skiplist_node *const> skiplist_node::forward() const noexcept {
  auto *base =
      reinterpret_cast<const skiplist_node *const *>(reinterpret_cast<const std::byte *>(this) + sizeof(skiplist_node));
  return {base, height_};
}

std::string_view skiplist_node::key() const noexcept {
  return {reinterpret_cast<const char *>(key_bytes().data()), key_size_};
}

std::string_view skiplist_node::value() const noexcept {
  return {reinterpret_cast<const char *>(value_bytes().data()), value_size_};
}

std::uint32_t skiplist_node::height() const noexcept { return height_; }

std::span<std::byte> skiplist_node::key_bytes() noexcept {
  auto *base = reinterpret_cast<std::byte *>(this) + sizeof(skiplist_node) + height_ * sizeof(skiplist_node *);
  return {base, key_size_};
}

std::span<const std::byte> skiplist_node::key_bytes() const noexcept {
  auto *base = reinterpret_cast<const std::byte *>(this) + sizeof(skiplist_node) + height_ * sizeof(skiplist_node *);
  return {base, key_size_};
}

std::span<std::byte> skiplist_node::value_bytes() noexcept {
  auto *base =
      reinterpret_cast<std::byte *>(this) + sizeof(skiplist_node) + height_ * sizeof(skiplist_node *) + key_size_;
  return {base, value_size_};
}

std::span<const std::byte> skiplist_node::value_bytes() const noexcept {
  auto *base =
      reinterpret_cast<const std::byte *>(this) + sizeof(skiplist_node) + height_ * sizeof(skiplist_node *) + key_size_;
  return {base, value_size_};
}

}  // namespace frankie::storage
