#include <cstdint>

#include "core/status.hpp"
#include "storage/sstable_format.hpp"

namespace frankie::storage {

[[nodiscard]] std::expected<std::span<index_entry>, core::status> decode_index(core::buffer_reader &reader,
                                                                               core::arena &arena) {
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
  if (!reader.read(index_offset).read(index_size).error()) {
    return core::unexpected(core::status_code::corrupted);
  }
  return sstable_footer{.index_offset_ = index_offset, .index_size_ = index_size};
}

}  // namespace frankie::storage
