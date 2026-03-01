//
// Created by nikon on 3/1/26.
//

#ifndef FRANKIE_SKIPLIST_PMR_H
#define FRANKIE_SKIPLIST_PMR_H

#include <cstring>
#include <expected>
#include <memory_resource>
#include <random>
#include <span>
#include <string_view>
#include <utility>

namespace frankie::storage::pmr {

// ---------------------------------------------------------------------------
// Error type
// ---------------------------------------------------------------------------
enum class error { not_found, alloc_failed };

// ---------------------------------------------------------------------------
// Comparator concept — replaces function pointer or unconstrained template
// ---------------------------------------------------------------------------
template <typename C>
concept Comparator = requires(const C &cmp, std::string_view a, std::string_view b) {
  { cmp(a, b) } -> std::convertible_to<int>;
};

// Default comparator using the spaceship operator
struct default_comparator {
  constexpr int operator()(std::string_view a, std::string_view b) const noexcept {
    auto cmp = a <=> b;
    if (cmp < 0) return -1;
    if (cmp > 0) return 1;
    return 0;
  }
};

static_assert(Comparator<default_comparator>);

class arena_resource : public std::pmr::memory_resource {
 public:
  static constexpr std::size_t kDefaultBlockSize = 4096 * 8;  // 32KB

  explicit arena_resource(std::size_t block_size = kDefaultBlockSize) noexcept : block_size_{block_size} {}

  arena_resource(const arena_resource &) = delete;
  arena_resource &operator=(const arena_resource &) = delete;
  arena_resource(arena_resource &&) = delete;
  arena_resource &operator=(arena_resource &&) = delete;

  [[nodiscard]] std::size_t bytes_allocated() const noexcept { return bytes_allocated_; }

  void release() noexcept {
    while (current_) {
      auto *prev = current_->next;
      ::operator delete(current_);
      current_ = prev;
    }
    offset_ = 0;
    bytes_allocated_ = 0;
  }

 private:
  struct block {
    block *next;

    char *data() noexcept { return reinterpret_cast<char *>(this) + sizeof(block); };
  };

  void *do_allocate(std::size_t bytes, std::size_t alignment) override {
    offset_ = (offset_ + alignment - 1) & ~(alignment - 1);

    if (!current_ || offset_ + bytes > block_size_) {
      auto capacity = std::max(block_size_, bytes);
      auto *blk = static_cast<block *>(::operator new(sizeof(block) + capacity));
      blk->next = current_;
      current_ = blk;
      offset_ = 0;
    }

    void *ptr = current_->data() + offset_;
    offset_ += bytes;
    bytes_allocated_ += bytes;
    return ptr;
  }

  void do_deallocate(void *, std::size_t, std::size_t) override {}

  [[nodiscard]] bool do_is_equal(const memory_resource &other) const noexcept override { return this == &other; }

  block *current_{nullptr};
  std::size_t block_size_{kDefaultBlockSize};
  std::size_t offset_{0};
  std::size_t bytes_allocated_{0};
};

class skiplist_node {
 public:
  // Factory — placement-new into arena memory
  [[nodiscard]] static skiplist_node *create(std::pmr::memory_resource *alloc, std::string_view key,
                                             std::string_view value, std::uint32_t height) noexcept {
    const auto size = layout_size(height, key.size(), value.size());
    void *mem = alloc->allocate(size, alignof(skiplist_node));
    std::memset(mem, 0, size);

    auto *node = ::new (mem) skiplist_node{};
    node->key_size_ = static_cast<std::uint32_t>(key.size());
    node->value_size_ = static_cast<std::uint32_t>(value.size());
    node->height_ = height;

    auto fwd = node->forward();
    std::ranges::fill(fwd, nullptr);

    auto key_dst = node->key_bytes();
    std::memcpy(key_dst.data(), key.data(), key.size());

    auto val_dst = node->value_bytes();
    std::memcpy(val_dst.data(), value.data(), value.size());

    return node;
  }

  // --- Accessors returning safe, typed views ---

  [[nodiscard]] std::span<skiplist_node *> forward() noexcept {
    auto *base = reinterpret_cast<skiplist_node **>(reinterpret_cast<std::byte *>(this) + sizeof(skiplist_node));
    return {base, height_};
  }

  [[nodiscard]] std::span<const skiplist_node *const> forward() const noexcept {
    auto *base = reinterpret_cast<const skiplist_node *const *>(reinterpret_cast<const std::byte *>(this) +
                                                                sizeof(skiplist_node));
    return {base, height_};
  }

  [[nodiscard]] std::string_view key() const noexcept {
    return {reinterpret_cast<const char *>(key_bytes().data()), key_size_};
  }

  [[nodiscard]] std::string_view value() const noexcept {
    return {reinterpret_cast<const char *>(value_bytes().data()), value_size_};
  }

  [[nodiscard]] std::uint32_t height() const noexcept { return height_; }

 private:
  std::uint32_t key_size_ = 0;
  std::uint32_t value_size_ = 0;
  std::uint32_t height_ = 0;

  [[nodiscard]] static constexpr std::size_t layout_size(std::uint32_t h, std::size_t ksz, std::size_t vsz) noexcept {
    return sizeof(skiplist_node) + h * sizeof(skiplist_node *) + ksz + vsz;
  }

  [[nodiscard]] std::span<std::byte> key_bytes() noexcept {
    auto *base = reinterpret_cast<std::byte *>(this) + sizeof(skiplist_node) + height_ * sizeof(skiplist_node *);
    return {base, key_size_};
  }

  [[nodiscard]] std::span<const std::byte> key_bytes() const noexcept {
    auto *base = reinterpret_cast<const std::byte *>(this) + sizeof(skiplist_node) + height_ * sizeof(skiplist_node *);
    return {base, key_size_};
  }

  [[nodiscard]] std::span<std::byte> value_bytes() noexcept {
    auto *base =
        reinterpret_cast<std::byte *>(this) + sizeof(skiplist_node) + height_ * sizeof(skiplist_node *) + key_size_;
    return {base, value_size_};
  }

  [[nodiscard]] std::span<const std::byte> value_bytes() const noexcept {
    auto *base = reinterpret_cast<const std::byte *>(this) + sizeof(skiplist_node) + height_ * sizeof(skiplist_node *) +
                 key_size_;
    return {base, value_size_};
  }
};

// ---------------------------------------------------------------------------
// Skiplist — RAII, comparator via concept, range-for support
// ---------------------------------------------------------------------------
template <Comparator Cmp = default_comparator>
class skiplist {
 public:
  static constexpr std::uint32_t kMaxHeight = 12;
  static constexpr std::uint32_t kBranchingFactor = 4;

  explicit skiplist(Cmp cmp = {}) noexcept : cmp_{cmp} { head_ = skiplist_node::create(&arena_, {}, {}, kMaxHeight); }

  // RAII: arena destructor frees everything
  ~skiplist() = default;

  // Non-copyable
  skiplist(const skiplist &) = delete;
  skiplist &operator=(const skiplist &) = delete;

  // Movable
  skiplist(skiplist &&) = delete;
  // skiplist(skiplist&& other) noexcept
  //       : arena_{std::move(other.arena_)}  // NOTE: arena_resource is non-movable,
  //                                          // so this wouldn't compile — see discussion
  //       , head_{std::exchange(other.head_, nullptr)}
  //       , current_height_{other.current_height_}
  //       , count_{other.count_}
  //       , rng_{other.rng_}
  //       , cmp_{other.cmp_}
  //   {}

  // --- Mutations ---

  void insert(std::string_view key, std::string_view value) noexcept {
    skiplist_node *update[kMaxHeight];
    auto *current = head_;

    for (int level = static_cast<int>(current_height_) - 1; level >= 0; --level) {
      auto lvl = static_cast<std::size_t>(level);
      while (current->forward()[lvl] && cmp_(current->forward()[lvl]->key(), key) < 0) {
        current = current->forward()[lvl];
      }
      update[level] = current;
    }

    auto *existing = current->forward()[0];
    if (existing && cmp_(existing->key(), key) == 0) {
      auto h = existing->height();
      auto *replacement = skiplist_node::create(&arena_, key, value, h);
      for (std::uint32_t i = 0; i < h; ++i) {
        replacement->forward()[i] = existing->forward()[i];
        update[i]->forward()[i] = replacement;
      }
      return;
    }

    auto height = random_height();
    if (height > current_height_) {
      for (auto i = current_height_; i < height; ++i) update[i] = head_;
      current_height_ = height;
    }

    auto *node = skiplist_node::create(&arena_, key, value, height);
    for (std::uint32_t i = 0; i < height; ++i) {
      node->forward()[i] = update[i]->forward()[i];
      update[i]->forward()[i] = node;
    }
    ++count_;
  }

  // --- Lookups ---

  [[nodiscard]] std::expected<std::string_view, error> get(std::string_view key) const noexcept {
    auto *current = head_;

    for (int level = static_cast<int>(current_height_) - 1; level >= 0; --level) {
      auto lvl = static_cast<std::size_t>(level);
      while (current->forward()[lvl] && cmp_(current->forward()[lvl]->key(), key) < 0) {
        current = current->forward()[lvl];
      }
    }

    auto *candidate = current->forward()[0];
    if (candidate && cmp_(candidate->key(), key) == 0) return candidate->value();

    return std::unexpected(error::not_found);
  }

  // --- Capacity ---

  [[nodiscard]] std::size_t size() const noexcept { return count_; }
  [[nodiscard]] std::size_t bytes_allocated() const noexcept { return arena_.bytes_allocated(); }

  // --- Range-for support via sentinel iterator ---

  class iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<std::string_view, std::string_view>;

    iterator() = default;
    explicit iterator(skiplist_node *n) : node_{n} {}

    value_type operator*() const { return {node_->key(), node_->value()}; }

    iterator &operator++() {
      node_ = node_->forward()[0];
      return *this;
    }
    iterator operator++(int) {
      auto tmp = *this;
      ++*this;
      return tmp;
    }

    bool operator==(std::default_sentinel_t) const { return node_ == nullptr; }

   private:
    skiplist_node *node_ = nullptr;
  };

  [[nodiscard]] iterator begin() const { return iterator{head_->forward()[0]}; }
  [[nodiscard]] std::default_sentinel_t end() const { return {}; }

 private:
  std::uint32_t random_height() noexcept {
    std::uint32_t h = 1;
    while (h < kMaxHeight && (dist_(rng_) % kBranchingFactor) == 0) ++h;
    return h;
  }

  // NOTE: arena_resource is declared mutable to allow get() to be const
  // even though pmr::memory_resource::allocate isn't const.
  // In practice, get() doesn't allocate — this is a known PMR friction point.
  mutable arena_resource arena_;
  skiplist_node *head_ = nullptr;
  std::uint32_t current_height_ = 1;
  std::size_t count_ = 0;

  std::mt19937 rng_{std::random_device{}()};
  std::uniform_int_distribution<std::uint32_t> dist_{0, UINT32_MAX};
  [[no_unique_address]] Cmp cmp_;
};

}  // namespace frankie::storage::pmr

#endif  // FRANKIE_SKIPLIST_PMR_H
