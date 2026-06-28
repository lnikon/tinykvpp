#include "storage/segment.hpp"
#include "core/status.hpp"
#include "storage/sstable_reader.hpp"

namespace frankie::storage {

[[nodiscard]] std::expected<segment, core::status> segment::create(std::filesystem::path path) noexcept {
  if (!std::filesystem::exists(path)) {
    return core::unexpected(core::status_code::not_found);
  }

  auto segment_file = core::random_access_file::open_read(path);
  if (!segment_file) {
    return core::unexpected(segment_file.error());
  }

  auto reader = sstable_reader::create(std::move(*segment_file));

  const auto sst_footer_or = reader->read_footer();
  if (!sst_footer_or.has_value()) {
    return core::unexpected(sst_footer_or.error());
  }

  core::arena index_arena = core::arena::create(sst_footer_or->index_size_);
  const auto index_entries_or = reader->read_index(*sst_footer_or, index_arena);

  segment result;
  result.index_arena_ = std::move(index_arena);
  result.index_entries_ = *index_entries_or;
  result.path_ = std::move(path);
  result.footer_ = *sst_footer_or;
  return result;
}

std::expected<kv_entry, core::status> segment::get_record(std::string_view key) noexcept {
  const auto index_entry = get_record_offset(key);
  if (!index_entry) {
    return core::unexpected(core::status_code::not_found);
  }
  // TODO(lnikon): Read data block at index_entry.data_block_offset_ and binary search inside it
  return {};
}

std::optional<index_entry> segment::get_record_offset(std::string_view key) const noexcept {
  const auto index_entry_it =
      std::lower_bound(index_entries_.begin(), index_entries_.end(), key,
                       [](const index_entry &entry, std::string_view rhs) { return entry.smallest_key_ < rhs; });

  // TODO(lnikon): Should use comparator from memtable here?
  if (index_entry_it != index_entries_.end() && index_entry_it->smallest_key_ == key) {
    return *index_entry_it;
  }
  return std::nullopt;
}

}  // namespace frankie::storage