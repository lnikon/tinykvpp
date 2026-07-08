#include <cstdint>

#include "core/assert.hpp"
#include "core/status.hpp"
#include "storage/sstable_format.hpp"

namespace frankie::storage {

[[nodiscard]] std::string_view encode_kv_entry(core::scratch_arena &arena, const internal_key &ikey) noexcept {
  arena.reset();

  const std::uint64_t total = ikey.user_key_.size() + internal_key::kMetadataSize;
  char *buf = arena.allocate(total);
  std::memset(buf, 0, total);
  char *cursor = buf;
  std::memcpy(cursor, ikey.user_key_.data(), ikey.user_key_.size());
  cursor += ikey.user_key_.size();
  std::memcpy(cursor, &ikey.sequence_, sizeof(ikey.sequence_));
  cursor += sizeof(ikey.sequence_);
  std::memcpy(cursor, &ikey.timestamp_, sizeof(ikey.timestamp_));
  cursor += sizeof(ikey.timestamp_);
  *cursor++ = static_cast<char>(ikey.tombstone_);

  return std::string_view{buf, total};
}

[[nodiscard]] internal_key decode_kv_entry(std::string_view encoded) noexcept {
  FR_VERIFY(encoded.size() >= internal_key::kMetadataSize);
  const auto user_key_size = encoded.size() - internal_key::kMetadataSize;

  internal_key result;
  result.user_key_ = encoded.substr(0, user_key_size);
  std::memcpy(&result.sequence_, encoded.data() + user_key_size, sizeof(result.sequence_));
  std::memcpy(&result.timestamp_, encoded.data() + user_key_size + sizeof(result.sequence_), sizeof(result.timestamp_));
  std::memcpy(&result.tombstone_, encoded.data() + user_key_size + sizeof(result.sequence_) + sizeof(result.timestamp_),
              sizeof(bool));
  return result;
}

[[nodiscard]] std::expected<sstable_data_block_header, core::status> decode_data_block_header(
    core::buffer_reader &reader) noexcept {
  (void)reader;
  return {};
}

[[nodiscard]] std::expected<sstable_data_block, core::status> decode_data_block(
    core::buffer_reader &reader, sstable_data_block_header header) noexcept {
  (void)reader;
  (void)header;
  return {};
}

[[nodiscard]] std::uint64_t kv_entry_allocated_bytes_count(const kv_entry &entry) noexcept {
  return entry.key_.size() + entry.value_.size() + internal_key::kMetadataSize;
}

[[nodiscard]] std::expected<std::span<index_entry>, core::status> decode_index(core::buffer_reader &reader,
                                                                               core::arena &arena) noexcept {
  std::uint32_t entries_count{0};
  if (auto result = reader.read(entries_count).error(); result) {
    return core::unexpected(result.value());
  }

  const auto requested_capacity = entries_count * sizeof(index_entry);
  auto *entries = static_cast<index_entry *>(arena.allocate(requested_capacity, alignof(index_entry)));
  if (entries == nullptr) {
    return core::unexpected(core::status_code::out_of_memory);
  }
  std::start_lifetime_as_array<index_entry>(entries, entries_count);

  for (std::uint32_t i = 0; i < entries_count; i++) {
    std::span<const std::byte> key_span;
    std::uint64_t key_len{0};
    std::uint64_t offset{0};
    std::uint64_t size{0};

    (void)reader.read_varint(key_len);
    (void)reader.read_bytes(key_len, key_span);
    (void)reader.read(offset).read(size);

    entries[i] = index_entry{
        .smallest_key_ = core::to_string_view(key_span), .data_block_offset_ = offset, .data_block_size_ = size};
  }
  if (reader.has_error()) {
    return core::unexpected(core::status_code::corrupted);
  }

  return std::span(entries, entries_count);
}

[[nodiscard]] std::expected<sstable_footer, core::status> decode_footer(core::buffer_reader &reader) noexcept {
  std::uint32_t index_offset{0};
  std::uint32_t index_size{0};
  if (reader.read(index_offset).read(index_size).error()) {
    return core::unexpected(core::status_code::corrupted);
  }
  return sstable_footer{.index_offset_ = index_offset, .index_size_ = index_size};
}

}  // namespace frankie::storage
