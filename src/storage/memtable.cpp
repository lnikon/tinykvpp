#include <cstring>
#include <utility>

#include "core/scratch_arena.hpp"
#include "core/status.hpp"
#include "core/time.hpp"
#include "storage/memtable.hpp"
#include "storage/sstable_format.hpp"

namespace frankie::storage {

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
      .user_key_ = key,
      .sequence_ = sequence,
      .timestamp_ = core::wall_clock_ms(),
      .tombstone_ = is_tombstone,
  };

  const std::string_view ikey_encoded = storage::encode_kv_entry(scratch_arena_, ikey);
  skiplist_.insert(ikey_encoded, value);

  count_++;
  bytes_allocated_ += ikey_encoded.size();
}

std::expected<kv_entry, core::status> memtable::get(const std::string_view key) const noexcept {
  auto lookup_ikey = storage::internal_key{
      .user_key_ = key,
      .sequence_ = 0,
      .timestamp_ = 0,
      .tombstone_ = false,
  };
  if (auto kv_opt = skiplist_.get(encode_kv_entry(scratch_arena_, lookup_ikey)); kv_opt) {
    auto decoded_ikey = decode_kv_entry(kv_opt->first);
    return kv_entry{
        .key_ = decoded_ikey.user_key_,
        .value_ = kv_opt->second,
        .sequence_ = decoded_ikey.sequence_,
        .timestamp_ = decoded_ikey.timestamp_,
        .tombstone_ = decoded_ikey.tombstone_,
    };
  }
  return core::unexpected(core::status_code::not_found);
}

std::uint64_t memtable::count() const noexcept { return count_; }

std::uint64_t memtable::bytes_allocated() const noexcept { return skiplist_.bytes_allocated(); }

std::uint64_t memtable::capacity() const noexcept { return capacity_; }

}  // namespace frankie::storage
