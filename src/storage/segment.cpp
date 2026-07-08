#include "storage/segment.hpp"
#include "core/assert.hpp"
#include "core/fs.hpp"
#include "core/scratch_arena.hpp"
#include "core/status.hpp"
#include "storage/sstable_format.hpp"
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
  if (!sst_footer_or) {
    return core::unexpected(sst_footer_or.error());
  }

  core::arena arena = core::arena::create(sst_footer_or->index_size_);
  const auto index_entries_or = reader->read_index(arena, *sst_footer_or);

  segment result;
  result.arena_ = std::move(arena);
  result.scratch_arena_ = core::scratch_arena{};
  result.index_entries_ = *index_entries_or;
  result.path_ = std::move(path);
  result.footer_ = *sst_footer_or;
  return result;
}

std::expected<kv_entry, core::status> segment::get_kv_entry(std::string_view user_key) noexcept {
  FR_VERIFY(!path_.empty());
  FR_VERIFY(!index_entries_.empty());

  const auto index_entry = get_record_offset(user_key);
  if (!index_entry) {
    return core::unexpected(core::status_code::not_found);
  }

  auto segment_file = core::random_access_file::open_read(path_);
  if (!segment_file) {
    return core::unexpected(segment_file.error());
  }
  auto reader = sstable_reader::create(std::move(*segment_file));

  auto data_block_or = reader->read_data_block(arena_, *index_entry);
  if (!data_block_or) {
    return core::unexpected(data_block_or.error());
  }

  const auto ikey = encode_kv_entry(scratch_arena_, storage::internal_key{
                                                        .user_key_ = user_key,
                                                        .sequence_ = 0,
                                                        .timestamp_ = 0,
                                                        .tombstone_ = false,
                                                    });

  const auto kv_entries_it = std::lower_bound(data_block_or->entries_.begin(), data_block_or->entries_.end(), ikey,
                                              [](std::string_view encoded_entry, std::string_view rhs) {
                                                return internal_key_comparator{}(encoded_entry, rhs);
                                              });

  if (kv_entries_it != data_block_or->entries_.end()) {
    // TODO(lnikon): Should also use sstable_data_block_entry_decoder (tmp name) here to get value as well
    auto decoded_ikey = decode_kv_entry(*kv_entries_it);
    return kv_entry{
        .key_ = decoded_ikey.user_key_,
        .value_ = kv_opt->second,
        .sequence_ = decoded_ikey.sequence_,
        .timestamp_ = decoded_ikey.timestamp_,
        .tombstone_ = decoded_ikey.tombstone_,
    };
  }

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
