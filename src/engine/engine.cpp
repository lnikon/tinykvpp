#include <optional>
#include <print>
#include <type_traits>

#include "engine/engine.hpp"
#include "storage/memtable.hpp"
#include "storage/skiplist.hpp"

namespace frankie::engine {

// Worst-case per-entry overhead from skiplist node allocation:
// sizeof(skiplist_node) + kMaxHeight * sizeof(pointer) + alignment padding
static constexpr std::uint64_t kNodeOverheadEstimate =
    sizeof(storage::skiplist_node) + storage::skiplist<storage::internal_key_comparator>::kMaxHeight * sizeof(void *);

std::optional<engine> engine::create(std::filesystem::path wal_path, const std::uint64_t memtable_capacity,
                                     const std::uint64_t wal_capacity) noexcept {
  engine result;

  result.memtable_capacity_ = memtable_capacity;
  result.memtable_active_ = storage::memtable::create(memtable_capacity);

  if (auto wal_reader = wal_reader::open(wal_path); wal_reader) {
    for (auto entry = wal_reader->read(); entry.has_value(); entry = wal_reader->read()) {
      if (!entry.has_value()) [[unlikely]] {
        break;
      }

      const auto &wal_entry = entry.value();
      switch (wal_entry.operation_) {
        case wal_operation::put:
          result.memtable_active_.put(wal_entry.key_, wal_entry.value_, wal_entry.sequence_, wal_entry.tombstone_);
          break;
        case wal_operation::del:
          result.memtable_active_.put(wal_entry.key_, "", wal_entry.sequence_, wal_entry.tombstone_);
          break;
        default:
          std::println("missing case handler for wal operation. op={}",
                       static_cast<std::underlying_type_t<wal_operation>>(wal_entry.operation_));
          break;
      }
    }
  }

  if (auto wal_writer_opt = wal_writer::open(std::move(wal_path), wal_capacity); wal_writer_opt.has_value()) {
    result.wal_ = std::move(wal_writer_opt.value());
  } else {
    return std::nullopt;
  }

  return result;
}

bool engine::put(std::string_view key, std::string_view value) noexcept {
  const auto sequence = get_next_sequence();

  if (!wal_.append({
          .operation_ = wal_operation::put,
          .sequence_ = sequence,
          .key_ = key,
          .value_ = value,
          .tombstone_ = false,
      })) {
    std::println("engine::put: failed to append wal. key={}, value={}, sequence={}", key, value, sequence);
    return false;
  }

  const std::uint64_t entry_bytes =
      key.size() + value.size() + storage::internal_key::kMetadataSize + kNodeOverheadEstimate;
  maybe_rotate_memtable(entry_bytes);

  memtable_active_.put(key, value, sequence, false);

  return true;
}

std::optional<std::string_view> engine::get(std::string_view key) noexcept {
  auto entry = memtable_active_.get(key);
  if (entry.has_value()) {
    if (entry.value().tombstone_) {
      return std::nullopt;
    }
    return entry.value().value();
  }

  if (memtable_immutable_.has_value()) {
    entry = memtable_immutable_->get(key);
    if (entry.has_value() && !entry.value().tombstone_) {
      return entry.value().value();
    }
  }

  return std::nullopt;
}

bool engine::del(std::string_view key) noexcept {
  const auto sequence = get_next_sequence();

  if (!wal_.append({
          .operation_ = wal_operation::del,
          .sequence_ = sequence,
          .key_ = key,
          .value_ = "",
          .tombstone_ = true,
      })) {
    std::println("engine::del: failed to append wal. key={} sequence={}", key, sequence);
    return false;
  }

  const std::uint64_t entry_bytes = key.size() + storage::internal_key::kMetadataSize + kNodeOverheadEstimate;
  maybe_rotate_memtable(entry_bytes);

  memtable_active_.put(key, {}, sequence, true);

  return true;
}

// TODO(lnikon): Interface tdb
void engine::scan(std::string_view range_start_key, std::string_view range_end_key) noexcept {
  (void)range_start_key;
  (void)range_end_key;
}

void engine::maybe_rotate_memtable(const std::uint64_t incoming_bytes) noexcept {
  if (memtable_active_.bytes_allocated() + incoming_bytes <= memtable_capacity_) {
    return;
  }
  memtable_immutable_.emplace(std::move(memtable_active_));
  memtable_active_ = storage::memtable::create(memtable_capacity_);
  // TODO(lnikon): Trigger flush of memtable_immutable_ to SST

  if (!wal_.truncate()) {
    std::println("engine::maybe_rotate_memtable: failed to truncate wal.");
    // TODO(lnikon): Need a fallback strategy e.g. move current wal into wal.old1 and create a new wal
    return;
  }
}

std::uint64_t engine::get_next_sequence() noexcept { return sequence_++; }

}  // namespace frankie::engine
