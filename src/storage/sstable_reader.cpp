#include "storage/sstable_reader.hpp"
#include "core/fs.hpp"
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

std::expected<std::span<index_entry>, core::status> sstable_reader::read_index(const sstable_footer &footer,
                                                                               core::arena &arena) noexcept {
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

std::expected<std::string_view, core::status> sstable_reader::get_data_block() noexcept {
  (void)file_;
  return {};
}

}  // namespace frankie::storage
