#include "storage/memtable.hpp"

namespace frankie::storage {

void memtable::put(std::string_view key, const std::string_view value, const std::uint64_t sequence,
                   const bool is_tombstone) noexcept {
  kv_entry entry;
  entry.key_ = key;
  entry.value_ = value;
  entry.sequence_ = sequence;
  entry.timestamp_ = sequence;  // NOTE: Use monotonic timestamp in epoch format with ms precision
  entry.tombstone_ = is_tombstone;

  std::array<char, 8> sequence_bytes;
  std::array<char, 8> timestamp_bytes;

  std::memcpy(sequence_bytes.data(), &sequence, sizeof(sequence));
  std::memcpy(timestamp_bytes.data(), &sequence, sizeof(sequence));
  char tombstone_byte = static_cast<char>(is_tombstone);

  std::string_view parts[] = {
    key,
    {sequence_bytes.data(), sizeof(sequence)},
    {timestamp_bytes.data(), sizeof(sequence)},
    {&tombstone_byte, 1},
  };

  skiplist_.insert(parts, entry.value());
  count_++;
}

std::optional<std::string_view> memtable::get(const std::string_view key) const noexcept {
  auto ret = skiplist_.get(key);
  if (!ret.has_value()) {
    return std::nullopt;
  }
  return ret.value();
}

std::uint64_t memtable::count() const noexcept { return count_; }

}  // namespace frankie::storage
