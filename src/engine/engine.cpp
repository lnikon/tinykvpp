#include "engine/engine.hpp"
#include "storage/memtable.hpp"
#include "storage/skiplist.hpp"

namespace frankie::engine {

// Worst-case per-entry overhead from skiplist node allocation:
// sizeof(skiplist_node) + kMaxHeight * sizeof(pointer) + alignment padding
static constexpr std::uint64_t kNodeOverheadEstimate =
    sizeof(storage::skiplist_node) +
    storage::skiplist<storage::internal_key_comparator>::kMaxHeight * sizeof(void *);

engine engine::create(const std::uint64_t memtable_capacity) noexcept {
  engine result;
  result.memtable_capacity_ = memtable_capacity;
  result.memtable_active_ = storage::memtable::create(memtable_capacity);
  return result;
}

void engine::put(std::string_view key, std::string_view value) noexcept {
  const std::uint64_t entry_bytes = key.size() + value.size() +
                                    storage::internal_key::kMetadataSize +
                                    kNodeOverheadEstimate;
  maybe_rotate_memtable(entry_bytes);
  memtable_active_.put(key, value, sequence_++, false);
}

std::optional<std::string_view> engine::get(std::string_view key) noexcept {
  const auto &entry = memtable_active_.get(key);
  if (entry.has_value() && !entry.value().tombstone_) {
    return entry.value().value();
  }
  // TODO(lnikon): Also check memtable_immutable_ before returning nullopt
  return std::nullopt;
}

void engine::del(std::string_view key) noexcept {
  const std::uint64_t entry_bytes = key.size() +
                                    storage::internal_key::kMetadataSize +
                                    kNodeOverheadEstimate;
  maybe_rotate_memtable(entry_bytes);
  memtable_active_.put(key, {}, sequence_++, true);
}

void engine::maybe_rotate_memtable(const std::uint64_t incoming_bytes) noexcept {
  if (memtable_active_.bytes_allocated() + incoming_bytes <= memtable_capacity_) {
    return;
  }
  memtable_immutable_.emplace(std::move(memtable_active_));
  memtable_active_ = storage::memtable::create(memtable_capacity_);
  // TODO(lnikon): Trigger flush of memtable_immutable_ to SST
}

// TODO(lnikon): Interface tdb
void engine::scan(std::string_view range_start_key, std::string_view range_end_key) noexcept {
  (void)range_start_key;
  (void)range_end_key;
}
}  // namespace frankie::engine
