#include "storage/memtable.hpp"

namespace frankie::storage {

std::string_view kv_entry::user_key() const noexcept { return key_; }

std::string_view kv_entry::internal_key(core::scratch_arena &arena) const noexcept {
  arena.reset();

  const std::uint64_t total = key_.size() + 8 + 8 + 1;
  char *buf = arena.allocate(total);

  char *p = buf;
  std::memcpy(p, key_.data(), key_.size());
  p += key_.size();
  std::memcpy(p, &sequence_, 8);
  p += 8;
  std::memcpy(p, &sequence_, 8);
  p += 8;
  *p++ = static_cast<char>(tombstone_);

  return std::string_view{buf, total};
}

std::string_view kv_entry::value() const noexcept { return value_; }

std::uint64_t kv_entry::bytes_allocated() const noexcept { return key_.size() + value_.size() + 8 + 8 + 1; }

memtable memtable::create(const std::uint64_t capacity) noexcept {
  memtable result;
  result.capacity_ = capacity;
  result.arena_ = core::arena::create(capacity);
  result.skiplist_ = skiplist<internal_key_comparator>::create(&result.arena_, internal_key_comparator{});
  return result;
}

void memtable::put(const std::string_view key, const std::string_view value, const std::uint64_t sequence,
                   const bool is_tombstone) noexcept {
  kv_entry entry;
  entry.key_ = key;
  entry.value_ = value;
  entry.sequence_ = sequence;
  entry.timestamp_ = sequence;  // NOTE: Use monotonic timestamp in epoch format with ms precision
  entry.tombstone_ = is_tombstone;

  skiplist_.insert(entry.internal_key(scratch_arena_), entry.value());

  count_++;
}

std::optional<std::string_view> memtable::get(const std::string_view key) const noexcept {
  kv_entry lookup;
  lookup.key_ = key;
  lookup.sequence_ = 0;
  lookup.timestamp_ = 0;
  lookup.tombstone_ = false;

  auto ret = skiplist_.get(lookup.internal_key(scratch_arena_));
  if (!ret.has_value()) {
    return std::nullopt;
  }
  return ret.value();
}

std::uint64_t memtable::count() const noexcept { return count_; }

}  // namespace frankie::storage
