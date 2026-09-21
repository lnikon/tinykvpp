#include "storage/sstable_writer.hpp"
#include "core/assert.hpp"
#include "core/status.hpp"
#include "core/views.hpp"
#include "storage/sstable_format.hpp"

namespace frankie::storage {

sstable_writer::~sstable_writer() noexcept { arena_.destroy(); }

// std::expected<void, core::status> sstable_writer::flush(std::ranges::range<std::pair<std::string_view, std::string_view>> range,
//                                                         core::random_access_file file, sstable_writer_options options) {
//   auto writer = sstable_writer::create(std::move(file), options);
//   if (!writer) {
//     return core::unexpected(writer.error());
//   }
// }

std::expected<sstable_writer, core::status> sstable_writer::create(core::random_access_file file, sstable_writer_options options) noexcept {
  const auto target_block_size = options.target_block_size_ + options.max_entry_size_;
  core::arena arena = core::arena::create(target_block_size);
  char *body_buffer = static_cast<char *>(arena.allocate(target_block_size, alignof(std::uint8_t)));
  if (body_buffer == nullptr) {
    return core::unexpected(core::status_code::out_of_memory);
  }

  sstable_writer result;
  result.options_ = options;
  result.file_ = std::move(file);
  result.arena_ = std::move(arena);
  // Size of the span acts as a capacity.
  result.data_block_span_ = core::to_writable_span(body_buffer, target_block_size);
  // Holding a pointer to body_span_ so that we do not have to re-create the writer after growing the body.
  result.data_block_span_writer_ = core::buffer_writer::create(result.data_block_span_);
  result.index_entries_.resize(kMaxIndexEntriesCount);
  return result;
}

std::expected<void, core::status> sstable_writer::add(std::string_view encoded_internal_key, std::string_view value) noexcept {
  FR_VERIFY(!data_block_span_.empty());

  const auto entry_size = sstable_data_block::calculate_entry_size(encoded_internal_key, value);
  if (entry_size == 0 || entry_size >= options_.max_entry_size_) {
    return core::unexpected(core::status_code::invalid_argument);
  }

  // don't let data_block grow over max target size.
  // add a new index entry and reset data block state (need an separate arena probably)
  if (data_block_size_bytes_ + entry_size >= options_.target_block_size_ + options_.max_entry_size_) {
    index_entries_.emplace_back(sstable_index_entry{.smallest_key_ = data_block_smallest_key_,
                                                    .data_block_offset_ = data_block_offset_relative_to_zero_,
                                                    .data_block_size_ = data_block_span_writer_.bytes_written()});
    // flush the block
    if (auto status = file_.write(core::to_string_view(data_block_span_), data_block_offset_relative_to_zero_); !status) {
      return core::unexpected(status.error());
    }
    // reset the block state
    data_block_span_writer_ = core::buffer_writer::create(data_block_span_);
    data_block_offset_relative_to_zero_ += data_block_size_bytes_;
    data_block_size_bytes_ = 0;
    data_block_entries_count_ = 0;
    data_block_smallest_key_ = std::string{};
  }

  // encode kv_entry into a block
  if (auto error = data_block_span_writer_.write(encoded_internal_key.size())
                       .write_bytes(core::to_span(encoded_internal_key))
                       .write(value.size())
                       .write_bytes(core::to_span(value))
                       .error();
      !error) {
    return core::unexpected(error.error());
  }
  data_block_size_bytes_ += entry_size;

  if (data_block_entries_count_ == 0) {
    data_block_smallest_key_ = encoded_internal_key.substr(0, encoded_internal_key.size() - internal_key::kMetadataSize);
  }
  data_block_entries_count_++;

  return {};
}

[[nodiscard]] std::expected<void, core::status> sstable_writer::finish() noexcept {
  const auto bytes_written_or = file_.write(core::to_string_view(data_block_span_), data_block_span_.size());
  if (!bytes_written_or) {
    return core::unexpected(bytes_written_or.error());
  }
  return {};
}

}  // namespace frankie::storage
