#include "storage/memtable.hpp"
#include <cstring>

namespace frankie::storage {

// ================================================================================
// internal_key
// ================================================================================

std::string_view internal_key::encode(core::scratch_arena &arena) const noexcept {
  arena.reset();

  const std::uint64_t total = user_key.size() + kMetadataSize;
  char *buf = arena.allocate(total);

  char *p = buf;
  std::memcpy(p, user_key.data(), user_key.size());
  p += user_key.size();
  std::memcpy(p, &sequence, 8);
  p += 8;
  std::memcpy(p, &timestamp, 8);
  p += 8;
  *p++ = static_cast<char>(tombstone);

  return std::string_view{buf, total};
}

internal_key internal_key::decode(const std::string_view encoded) noexcept {
  assert(encoded.size() >= kMetadataSize);
  const auto user_key_size = encoded.size() - kMetadataSize;

  internal_key result;
  result.user_key = encoded.substr(0, user_key_size);
  std::memcpy(&result.sequence, encoded.data() + user_key_size, 8);
  std::memcpy(&result.timestamp, encoded.data() + user_key_size + 8, 8);
  std::memcpy(&result.tombstone, encoded.data() + user_key_size + 8 + 8, 1);
  return result;
}

// ================================================================================
// kv_entry
// ================================================================================

std::string_view kv_entry::user_key() const noexcept { return key_; }

std::string_view kv_entry::value() const noexcept { return value_; }

std::uint64_t kv_entry::bytes_allocated() const noexcept { return key_.size() + value_.size() + 8 + 8 + 1; }

// ================================================================================
// memtable
// ================================================================================

memtable memtable::create(const std::uint64_t capacity) noexcept {
  memtable result;
  result.capacity_ = capacity;
  result.arena_ = core::arena::create(capacity);
  result.skiplist_ = skiplist<internal_key_comparator>::create(&result.arena_, internal_key_comparator{});
  return result;
}

void memtable::put(const std::string_view key, const std::string_view value, const std::uint64_t sequence,
                   const bool is_tombstone) noexcept {
  auto ikey = internal_key{
      .user_key = key,
      .sequence = sequence,
      .timestamp = sequence,  // NOTE: Use monotonic timestamp in epoch format with ms precision
      .tombstone = is_tombstone,
  };
  skiplist_.insert(ikey.encode(scratch_arena_), value);
  count_++;
}

std::optional<kv_entry> memtable::get(const std::string_view key) const noexcept {
  auto ikey = internal_key{
      .user_key = key,
      .sequence = 0,
      .timestamp = 0,
      .tombstone = false,
  };
  if (auto ret = skiplist_.get(ikey.encode(scratch_arena_)); ret.has_value()) {
    auto ik = internal_key::decode(ret->first);
    return kv_entry{
        .key_ = ik.user_key,
        .value_ = ret->second,
        .sequence_ = ik.sequence,
        .timestamp_ = ik.timestamp,
        .tombstone_ = ik.tombstone,
    };
  }
  return std::nullopt;
}

std::uint64_t memtable::count() const noexcept { return count_; }

[[nodiscard]] std::uint64_t memtable::bytes_allocated() const noexcept { return arena_.bytes_allocated(); }

}  // namespace frankie::storage
