#include <cstring>
#include <utility>

#include "core/scratch_arena.hpp"
#include "core/status.hpp"
#include "core/time.hpp"
#include "core/views.hpp"
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

std::expected<void, core::status> memtable::put(const std::string_view user_key, const std::string_view value, const std::uint64_t sequence,
                                                const bool is_tombstone) noexcept {
  auto ikey = internal_key{
      .user_key_ = user_key,
      .sequence_ = sequence,
      .timestamp_ = core::wall_clock_ms(),
      .tombstone_ = is_tombstone,
  };
  auto encoded_or = encode_internal_key(ikey);
  if (!encoded_or) {
    return core::unexpected(encoded_or.error());
  }

  skiplist_.insert(*encoded_or, value);

  count_++;
  bytes_allocated_ += ikey.bytes_required();

  return {};
}

std::expected<kv_entry, core::status> memtable::get(const std::string_view key) noexcept {
  auto ikey = storage::internal_key{
      .user_key_ = key,
      .sequence_ = 0,
      .timestamp_ = 0,
      .tombstone_ = false,
  };
  auto encoded_or = encode_internal_key(ikey);
  if (!encoded_or) {
    return core::unexpected(encoded_or.error());
  }

  if (auto kv_opt = skiplist_.get(*encoded_or); kv_opt) {
    core::buffer_reader reader(core::to_span(kv_opt->first));
    auto decoded_ikey = internal_key::decode(reader);
    if (!decoded_ikey) {
      return core::unexpected(core::status_code::corrupted);
    }
    return kv_entry{
        .key_ = decoded_ikey->user_key_,
        .value_ = kv_opt->second,
        .sequence_ = decoded_ikey->sequence_,
        .timestamp_ = decoded_ikey->timestamp_,
        .tombstone_ = decoded_ikey->tombstone_,
    };
  }

  return core::unexpected(core::status_code::not_found);
}

std::uint64_t memtable::count() const noexcept { return count_; }

std::uint64_t memtable::bytes_allocated() const noexcept { return skiplist_.bytes_allocated(); }

std::uint64_t memtable::capacity() const noexcept { return capacity_; }

memtable::iterator memtable::begin() { return skiplist_.begin(); }

std::default_sentinel_t memtable::end() { return skiplist_.end(); }

std::expected<std::string_view, core::status> memtable::encode_internal_key(const internal_key &ikey) noexcept {
  scratch_arena_.reset();
  auto *buffer = static_cast<char *>(scratch_arena_.allocate(ikey.bytes_required(), alignof(std::uint8_t)));
  if (buffer == nullptr) {
    return core::unexpected(core::status_code::out_of_memory);
  }
  auto buffer_span = core::to_writable_span(buffer, ikey.bytes_required());
  auto writer = core::buffer_writer::create(&buffer_span);

  if (const auto result = ikey.encode(writer); !result) {
    return core::unexpected(result.error());
  }

  return std::string_view{buffer, ikey.bytes_required()};
}

}  // namespace frankie::storage
