#include "storage/sstable_reader.hpp"
#include "core/assert.hpp"
#include "core/fs.hpp"
#include "core/serialization/buffer_reader.hpp"
#include "core/status.hpp"
#include "core/views.hpp"
#include "storage/sstable_format.hpp"

namespace frankie::storage {

sstable_reader::~sstable_reader() noexcept {}

std::expected<sstable_reader, core::status> sstable_reader::create(core::random_access_file file) noexcept {
  sstable_reader result;
  result.file_ = std::move(file);
  return result;
}

std::expected<sstable_footer, core::status> sstable_reader::read_footer() noexcept {
  const auto file_size = file_.size();
  if (!file_size) {
    return core::unexpected(file_size.error());
  }
  if (*file_size == 0) {
    return core::unexpected(core::status_code::corrupted);
  }

  std::array<char, 2 * sizeof(std::uint32_t)> buffer{};
  auto bytes_read_or = file_.read(buffer, *file_size - kFooterSize);
  if (!bytes_read_or) {
    return core::unexpected(bytes_read_or.error());
  }
  if (bytes_read_or != kFooterSize) {
    return core::unexpected(core::status_code::corrupted);
  }

  core::buffer_reader reader(core::to_writable_span(buffer.data(), buffer.size()));
  return decode_footer(reader);
}

std::expected<std::span<index_entry>, core::status> sstable_reader::read_index(core::arena &arena,
                                                                               const sstable_footer &footer) noexcept {
  char *index_buffer = static_cast<char *>(arena.allocate(footer.index_size_, alignof(std::uint8_t)));
  if (index_buffer == nullptr) {
    return core::unexpected(core::status_code::out_of_memory);
  }

  const auto bytes_read_or = file_.read(std::span(index_buffer, footer.index_size_), footer.index_offset_);
  if (!bytes_read_or) {
    return core::unexpected(bytes_read_or.error());
  }
  if (*bytes_read_or != footer.index_size_) {
    return core::unexpected(core::status_code::corrupted);
  }

  const auto index_buffer_span = core::to_writable_span(index_buffer, footer.index_size_);
  core::buffer_reader reader(index_buffer_span);
  return decode_index(reader, arena);
}

[[nodiscard]] std::expected<sstable_data_block, core::status> sstable_reader::read_data_block(
    core::arena &arena, const index_entry index) noexcept {
  FR_VERIFY(index.data_block_size_ != 0ULL);
  FR_VERIFY(!index.smallest_key_.empty());

  const auto data_block_size = index.data_block_size_;
  auto *data_block_buffer = static_cast<char *>(arena.allocate(data_block_size, sizeof(char)));
  auto data_block_span = std::span(data_block_buffer, data_block_size);

  const auto bytes_read_or = file_.read(data_block_span, index.data_block_offset_);
  if (!bytes_read_or) {
    return core::unexpected(core::status_code::io_error);
  }
  if (*bytes_read_or == 0 || *bytes_read_or != data_block_size) {
    return core::unexpected(core::status_code::corrupted);
  }

  core::buffer_reader reader(core::to_writable_span(data_block_span));
  const auto header_or = decode_data_block_header(reader);
  if (!header_or) {
    return core::unexpected(core::status_code::corrupted);
  }

  const auto data_block = decode_data_block(reader, *header_or);
  if (!data_block) {
    return core::unexpected(core::status_code::corrupted);
  }
  return *data_block;
}

}  // namespace frankie::storage
