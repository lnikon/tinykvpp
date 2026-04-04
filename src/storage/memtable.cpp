#include <cstring>

#include "core/time.hpp"
#include "storage/memtable.hpp"

namespace frankie::storage {

// ================================================================================
// internal_key
// ================================================================================

std::string_view internal_key::encode(core::scratch_arena &arena) const noexcept {
  arena.reset();

  const std::uint64_t total = user_key.size() + kMetadataSize;
  char *buf = arena.allocate(total);

  char *cursor = buf;
  std::memcpy(cursor, user_key.data(), user_key.size());
  cursor += user_key.size();
  std::memcpy(cursor, &sequence, sizeof(sequence));
  cursor += sizeof(sequence);
  std::memcpy(cursor, &timestamp, sizeof(timestamp));
  cursor += sizeof(timestamp);
  *cursor++ = static_cast<char>(tombstone);

  return std::string_view{buf, total};
}

internal_key internal_key::decode(const std::string_view encoded) noexcept {
  assert(encoded.size() >= kMetadataSize);
  const auto user_key_size = encoded.size() - kMetadataSize;

  internal_key result;
  result.user_key = encoded.substr(0, user_key_size);
  std::memcpy(&result.sequence, encoded.data() + user_key_size, sizeof(sequence));
  std::memcpy(&result.timestamp, encoded.data() + user_key_size + sizeof(sequence), sizeof(timestamp));
  std::memcpy(&result.tombstone, encoded.data() + user_key_size + sizeof(sequence) + sizeof(timestamp), 1);
  return result;
}

// ================================================================================
// kv_entry
// ================================================================================

std::string_view kv_entry::user_key() const noexcept { return key_; }

std::string_view kv_entry::value() const noexcept { return value_; }

std::uint64_t kv_entry::bytes_allocated() const noexcept {
  return key_.size() + value_.size() + internal_key::kMetadataSize;
}

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
      .timestamp = core::wall_clock_ms(),
      .tombstone = is_tombstone,
  };
  skiplist_.insert(ikey.encode(scratch_arena_), value);
  count_++;
}

std::optional<kv_entry> memtable::get(const std::string_view key) const noexcept {
  auto lookup_ikey = internal_key{
      .user_key = key,
      .sequence = 0,
      .timestamp = 0,
      .tombstone = false,
  };
  if (auto kv_opt = skiplist_.get(lookup_ikey.encode(scratch_arena_)); kv_opt.has_value()) {
    auto decoded_ikey = internal_key::decode(kv_opt->first);
    return kv_entry{
        .key_ = decoded_ikey.user_key,
        .value_ = kv_opt->second,
        .sequence_ = decoded_ikey.sequence,
        .timestamp_ = decoded_ikey.timestamp,
        .tombstone_ = decoded_ikey.tombstone,
    };
  }
  return std::nullopt;
}

std::uint64_t memtable::count() const noexcept { return count_; }

[[nodiscard]] std::uint64_t memtable::bytes_allocated() const noexcept { return arena_.bytes_allocated(); }

}  // namespace frankie::storage
