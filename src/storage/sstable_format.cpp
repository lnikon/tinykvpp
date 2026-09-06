#include <cstdint>
#include <string_view>

#include "core/status.hpp"
#include "core/views.hpp"
#include "storage/sstable_format.hpp"

namespace frankie::storage {

// Internal key
std::expected<void, core::status> internal_key::encode(core::buffer_writer &writer) const noexcept {
  return writer.write_bytes(core::to_span(user_key_)).write(sequence_).write(timestamp_).write(tombstone_).error();
}

std::expected<internal_key, core::status> internal_key::decode(core::arena &arena, core::buffer_reader &reader) noexcept {
  if (reader.remaining() < internal_key::kMetadataSize) {
    return core::unexpected(core::status_code::corrupted);
  }

  const auto user_key_size = reader.remaining() - internal_key::kMetadataSize;

  internal_key result;
  std::span<const std::byte> user_key_bytes;
  if (auto status = reader.read_bytes(user_key_size, user_key_bytes).read(result.sequence_).read(result.timestamp_).read(result.tombstone_).error();
      !status) {
    return core::unexpected(status.error());
  }

  auto *user_key_buf = static_cast<std::byte *>(arena.allocate(user_key_size, alignof(std::uint8_t)));
  std::memcpy(user_key_buf, user_key_bytes.data(), user_key_size);
  result.user_key_ = core::to_string_view(std::span<const std::byte>(user_key_buf, user_key_size));
  return result;
}

std::uint64_t internal_key::bytes_required() const noexcept { return user_key_.size() + kMetadataSize; }

// KV entry
[[nodiscard]] internal_key kv_entry::get_internal_key() const noexcept {
  return {
      .user_key_ = key_,
      .sequence_ = sequence_,
      .timestamp_ = timestamp_,
      .tombstone_ = tombstone_,
  };
}

[[nodiscard]] std::uint64_t kv_entry::bytes_count() const noexcept {
  return key_.size() + value_.size() + (2 * sizeof(std::size_t)) + internal_key::kMetadataSize;
}

[[nodiscard]] std::expected<void, core::status> kv_entry::encode(core::buffer_writer &writer) const noexcept {
  const auto internal_key = get_internal_key();
  auto error = internal_key.encode(writer);
  if (error) {
    return error;
  }
  error = writer.write_string(value_).error();
  return error;
}

std::expected<kv_entry, core::status> kv_entry::decode(core::arena &arena, core::buffer_reader &reader) noexcept {
  // Decode internal key.
  auto ikey_or = internal_key::decode(arena, reader);
  if (!ikey_or) {
    return core::unexpected(ikey_or.error());
  }

  // Get the size of the value.
  std::uint64_t value_size = 0;
  if (auto status = reader.read(value_size).error(); !status) {
    return core::unexpected(status.error());
  }

  // Allocate memory to store the value, prepare the span, and read the value into it.
  auto *value_buf = static_cast<char *>(arena.allocate(value_size, alignof(std::uint8_t)));
  if (value_buf == nullptr) {
    return core::unexpected(core::status_code::out_of_memory);
  }
  auto value_span = core::to_span(std::string_view(value_buf, value_size));
  if (auto status = reader.read_bytes(value_size, value_span).error(); !status) {
    return core::unexpected(status.error());
  }

  kv_entry result;
  result.key_ = ikey_or->user_key_;
  result.sequence_ = ikey_or->sequence_;
  result.timestamp_ = ikey_or->timestamp_;
  result.tombstone_ = ikey_or->tombstone_;
  result.value_ = core::to_string_view(value_span);
  return result;
}

/// Data block
[[nodiscard]] std::expected<void, core::status> sstable_data_block_header::encode(core::buffer_writer &writer) const noexcept {}

[[nodiscard]] std::expected<sstable_data_block_header, core::status> sstable_data_block_header::decode(core::buffer_reader &reader) noexcept {
  return {};
}

[[nodiscard]] static std::expected<sstable_data_block, core::status> sstable_data_block::decode(core::buffer_reader &reader, core::arena &arena,
                                                                                                const sstable_data_block_header &header) noexcept {}

[[nodiscard]] std::uint64_t sstable_data_block::calculate_entry_size(std::string_view ikey, std::string_view value) noexcept {
  return (sizeof(std::uint64_t) * 2) + ikey.size() + value.size();
}

/// Index
[[nodiscard]] std::expected<void, core::status> sstable_index_entry::encode(core::buffer_writer &writer) const noexcept {
  return core::unexpected(core::status_code::not_implemented);
}

[[nodiscard]] std::expected<std::span<sstable_index_entry>, core::status> sstable_index_entry::decode(core::buffer_reader &reader,
                                                                                                      core::arena &arena) noexcept {
  return core::unexpected(core::status_code::not_implemented);
}

[[nodiscard]] std::expected<void, core::status> sstable_index::encode(core::buffer_writer &writer) const noexcept {
  for (auto &entry : entries_) {
    if (auto result = entry.encode(writer); !result) {
      return result;
    }
  }
}

[[nodiscard]] std::expected<std::span<sstable_index>, core::status> sstable_index::decode(core::buffer_reader &reader, core::arena &arena) noexcept {
  // TODO(lnikon) No idea at the moment how to do this
  // std::uint32_t entries_count{0};
  // if (auto result = reader.read(entries_count).error(); !result) {
  //   return core::unexpected(result.error());
  // }

  // const auto requested_capacity = entries_count * sizeof(index_entry);
  // auto *entries = static_cast<index_entry *>(arena.allocate(requested_capacity, alignof(index_entry)));
  // if (entries == nullptr) {
  //   return core::unexpected(core::status_code::out_of_memory);
  // }
  // std::start_lifetime_as_array<index_entry>(entries, entries_count);

  // for (std::uint32_t i = 0; i < entries_count; i++) {
  //   std::span<const std::byte> key_span;
  //   std::uint64_t key_len{0};
  //   std::uint64_t offset{0};
  //   std::uint64_t size{0};

  //   (void)reader.read_varint(key_len);
  //   (void)reader.read_bytes(key_len, key_span);
  //   (void)reader.read(offset).read(size);

  //   entries[i] = index_entry{.smallest_key_ = core::to_string_view(key_span), .data_block_offset_ = offset, .data_block_size_ = size};
  // }
  // if (reader.has_error()) {
  //   return core::unexpected(core::status_code::corrupted);
  // }

  // return std::span(entries, entries_count);
}

std::expected<void, core::status> sstable_footer::encode(core::buffer_writer &writer) const noexcept {
  return writer.write(index_offset_).write(index_size_).write(bloom_offset_).write(bloom_size_).write(crc32_).error();
}

std::expected<sstable_footer, core::status> sstable_footer::decode(core::buffer_reader &reader) noexcept {
  std::uint32_t index_offset{0};
  std::uint32_t index_size{0};
  if (reader.read(index_offset).read(index_size).error()) {
    return core::unexpected(core::status_code::corrupted);
  }
  return sstable_footer{.index_offset_ = index_offset, .index_size_ = index_size};
}

}  // namespace frankie::storage
