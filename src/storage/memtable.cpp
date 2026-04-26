#include <cstring>
#include <utility>

#include "core/scratch_arena.hpp"
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
  std::memset(buf, 0, total);
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
  FR_VERIFY(encoded.size() >= kMetadataSize);
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

memtable::memtable(memtable &&other) noexcept
    : scratch_arena_{std::exchange(other.scratch_arena_, {})},
      arena_{std::exchange(other.arena_, {})},
      skiplist_{std::exchange(other.skiplist_, {})},
      count_{std::exchange(other.count_, 0)},
      capacity_{std::exchange(other.capacity_, 0)},
      bytes_allocated_{std::exchange(other.bytes_allocated_, 0)},
      min_sequence_{std::exchange(other.min_sequence_, 0)},
      max_sequence_{std::exchange(other.max_sequence_, 0)} {
  // skiplist_ carried over a raw pointer to the *source* arena; re-point it
  // at our own arena, which now owns the blocks the skiplist's nodes live in.
  skiplist_.rebind_arena(&arena_);
}

memtable &memtable::operator=(memtable &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  scratch_arena_ = std::exchange(other.scratch_arena_, {});
  arena_ = std::exchange(other.arena_, {});
  skiplist_ = std::exchange(other.skiplist_, {});
  count_ = std::exchange(other.count_, 0);
  capacity_ = std::exchange(other.capacity_, 0);
  bytes_allocated_ = std::exchange(other.bytes_allocated_, 0);
  min_sequence_ = std::exchange(other.min_sequence_, 0);
  max_sequence_ = std::exchange(other.max_sequence_, 0);

  skiplist_.rebind_arena(&arena_);

  return *this;
}

memtable memtable::create(const std::uint64_t capacity) noexcept {
  memtable result;
  result.capacity_ = capacity;
  result.arena_ = core::arena::create(capacity);
  result.skiplist_ = skiplist<internal_key_comparator>::create(&result.arena_, internal_key_comparator{});
  return result;
}

memtable::~memtable() {
  arena_.destroy();
  scratch_arena_.destroy();
}

void memtable::put(const std::string_view key, const std::string_view value, const std::uint64_t sequence,
                   const bool is_tombstone) noexcept {
  auto ikey = internal_key{
      .user_key = key,
      .sequence = sequence,
      .timestamp = core::wall_clock_ms(),
      .tombstone = is_tombstone,
  };

  const std::string_view ikey_encoded = ikey.encode(scratch_arena_);
  skiplist_.insert(ikey_encoded, value);

  count_++;
  bytes_allocated_ += ikey_encoded.size();
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

std::uint64_t memtable::bytes_allocated() const noexcept { return skiplist_.bytes_allocated(); }

std::uint64_t memtable::capacity() const noexcept { return capacity_; }

}  // namespace frankie::storage
