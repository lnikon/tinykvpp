#include <cstring>
#include <print>

#include "core/assert.hpp"
#include "core/serialization/buffer_writer.hpp"
#include "core/serialization/common.hpp"
#include "core/status.hpp"
#include "storage/sstable_format.hpp"
#include "storage/sstable_writer.hpp"

namespace frankie::storage {

sstable_writer::~sstable_writer() noexcept {
  current_block_state_.arena_.destroy();
  index_entries_state_.arena_.destroy();
}

std::expected<sstable_writer, core::status> sstable_writer::create(sstable_writer_config config) noexcept {
  FR_VERIFY_MSG(config.target_block_size_ > 0ULL, "sst block size should be greater than zero");

  auto data_block_arena = core::arena::create(kSSTableArenaCapacity);
  auto *data_block_body = static_cast<char *>(data_block_arena.allocate(kSSTableArenaCapacity, alignof(std::uint64_t)));
  if (data_block_body == nullptr) {
    return core::unexpected(core::status_code::out_of_memory);
  }
  auto data_block_buffer_writer =
      core::buffer_writer::create(core::to_writable_span(data_block_body, kSSTableArenaCapacity));

  sstable_writer result;
  result.gpa_arena_ = core::arena::create(1024);
  result.config_ = config;
  result.current_block_state_ = data_block_state{
      .arena_ = std::move(data_block_arena),
      .data_block_body_ = data_block_body,
      .data_block_size_ = 0,
      .data_block_capacity_ = kSSTableArenaCapacity,
      .entry_count_ = 0,
      .buffer_writer_ = std::move(data_block_buffer_writer),
  };

  auto index_arena = core::arena::create(kSSTableArenaCapacity);
  auto *index_entries = static_cast<index_entry *>(index_arena.allocate(kSSTableArenaCapacity, alignof(std::uint64_t)));
  if (index_entries == nullptr) {
    return core::unexpected(core::status_code::out_of_memory);
  }
  result.index_entries_state_ = {
      .arena_ = std::move(index_arena),
      .index_entries_ = index_entries,
      .index_entries_size_ = 0,
      .index_entries_capacity_ = kSSTableArenaCapacity,
      .entry_count_ = 0,
  };
  return result;
}

std::expected<void, core::status> sstable_writer::append(std::string_view ikey, std::string_view value) noexcept {
  FR_VERIFY(current_block_state_.data_block_body_ != nullptr);
  FR_VERIFY(current_block_state_.data_block_capacity_ != 0ULL);
  FR_VERIFY(index_entries_state_.index_entries_ != nullptr);
  FR_VERIFY(index_entries_state_.index_entries_capacity_ != 0ULL);

  const std::uint64_t entry_size = sizeof(std::uint32_t) + ikey.size() + sizeof(std::uint32_t) + value.size();

  // TODO(lnikon): Allocate more memory .. perfectly, arena should support re-allocation by itself.
  if (entry_size + current_block_state_.data_block_size_ >= current_block_state_.data_block_capacity_) {
    // Allocate double the existing capacity.
    const auto new_capacity = current_block_state_.data_block_capacity_ * 2;
    char *new_buffer = static_cast<char *>(current_block_state_.arena_.allocate(new_capacity, alignof(std::uint64_t)));
    if (new_buffer == nullptr) {
      return core::unexpected(core::status_code::out_of_memory);
    }

    std::memcpy(new_buffer, current_block_state_.data_block_body_, current_block_state_.data_block_size_);

    current_block_state_.data_block_body_ = new_buffer;
    current_block_state_.data_block_capacity_ = new_capacity;
    current_block_state_.buffer_writer_.set_buffer(
        core::to_writable_span(current_block_state_.data_block_body_, current_block_state_.data_block_capacity_));
  }

  // Save the smallest key for the index entry.
  if (current_block_state_.data_block_size_ == 0) {
    auto *smallest_key_buf = static_cast<char *>(index_entries_state_.arena_.allocate(
        sizeof(std::uint32_t) + ikey.size() + sizeof(std::uint32_t), alignof(std::uint64_t)));
    if (smallest_key_buf == nullptr) {
      return core::unexpected(core::status_code::out_of_memory);
    }
    std::memcpy(smallest_key_buf, ikey.data(), ikey.size());

    if (index_entries_state_.index_entries_size_ >= index_entries_state_.index_entries_capacity_) {
      // Allocate double the existing capacity.
      const auto new_capacity = index_entries_state_.index_entries_capacity_ * 2;
      auto *new_buffer =
          static_cast<index_entry *>(index_entries_state_.arena_.allocate(new_capacity, alignof(std::uint64_t)));
      if (new_buffer == nullptr) {
        return core::unexpected(core::status_code::out_of_memory);
      }

      // Copy index entries from the old buffer into the new one.
      for (std::size_t idx{0}; idx < index_entries_state_.index_entries_size_; idx++) {
        new_buffer[idx] = index_entries_state_.index_entries_[idx];
      }

      index_entries_state_.index_entries_ = new_buffer;
      index_entries_state_.index_entries_capacity_ = new_capacity;
    }

    index_entries_state_.index_entries_[index_entries_state_.index_entries_size_].smallest_key_ =
        std::string_view{smallest_key_buf, ikey.size()};
  }

  auto error_or = current_block_state_.buffer_writer_.write_string(ikey).write_string(value).error();
  if (error_or) {
    std::println("sstable_writer::append: buffer_writer failed. serialization_error={}",
                 static_cast<std::uint8_t>(*error_or));
    return core::unexpected(core::status_code::invalid_argument);
  }

  current_block_state_.entry_count_++;
  // TODO(lnikon): Need to refactor buffer writer to remove # of written bytes.
  current_block_state_.data_block_size_ += static_cast<std::uint32_t>(entry_size);

  return {};
}

bool sstable_writer::is_data_block_complete() const noexcept {
  return current_block_state_.data_block_size_ >= config_.target_block_size_;
}

std::expected<std::string_view, core::status> sstable_writer::get_data_block() noexcept {
  FR_VERIFY(current_block_state_.data_block_body_ != nullptr);
  FR_VERIFY(current_block_state_.data_block_capacity_ != 0ULL);
  FR_VERIFY(current_block_state_.data_block_size_ != 0ULL);
  FR_VERIFY(current_block_state_.entry_count_ != 0ULL);

  // Prepare data block header and body.
  const sstable_data_block_header data_block_header{
      .entry_count_ = current_block_state_.entry_count_,
      .uncompressed_size = current_block_state_.data_block_size_,
      .compressed_size = current_block_state_.data_block_size_,
      .compression_type = sstable_compression_type::none,
      .crc32_ = 0,
  };
  const std::string_view data_block_header_view{reinterpret_cast<const char *>(&data_block_header),
                                                sizeof(data_block_header)};
  const std::string_view data_block_view{current_block_state_.data_block_body_, current_block_state_.data_block_size_};

  // Allocate enough memory store data block header and body.
  const std::uint32_t data_block_image_size = sizeof(std::uint64_t) + sizeof(sstable_data_block_header) +
                                              sizeof(std::uint64_t) + current_block_state_.data_block_size_;
  char *data_block_image_buffer =
      static_cast<char *>(current_block_state_.arena_.allocate(data_block_image_size, alignof(std::uint64_t)));
  if (data_block_image_buffer == nullptr) {
    return core::unexpected(core::status_code::out_of_memory);
  }

  // Create buffer writer to serialize data blocks content.
  auto image_buffer_writer =
      core::buffer_writer::create(core::to_writable_span(data_block_image_buffer, data_block_image_size));

  // Serialize data block.
  auto error_or = image_buffer_writer.write_string(data_block_header_view).write_string(data_block_view).error();
  if (error_or) {
    return core::unexpected(core::status_code::corrupted);
  }

  return std::string_view{data_block_image_buffer, data_block_image_size};
}

std::expected<void, core::status> sstable_writer::record_data_block(std::uint64_t offset, std::uint64_t size) noexcept {
  FR_VERIFY(current_block_state_.data_block_body_ != nullptr);
  FR_VERIFY(current_block_state_.data_block_capacity_ != 0ULL);
  FR_VERIFY(current_block_state_.data_block_size_ <= current_block_state_.data_block_capacity_);
  FR_VERIFY(index_entries_state_.index_entries_ != nullptr);
  FR_VERIFY(index_entries_state_.index_entries_capacity_ != 0ULL);

  if (index_entries_state_.index_entries_size_ >= index_entries_state_.index_entries_capacity_) {
    // Allocate double the existing capacity.
    const auto new_capacity = index_entries_state_.index_entries_capacity_ * 2;
    auto *new_buffer =
        static_cast<index_entry *>(index_entries_state_.arena_.allocate(new_capacity, alignof(std::uint64_t)));
    if (new_buffer == nullptr) {
      return core::unexpected(core::status_code::out_of_memory);
    }

    // Copy index entries from the old buffer into the new one.
    for (std::size_t idx{0}; idx < index_entries_state_.index_entries_size_; idx++) {
      new_buffer[idx] = index_entries_state_.index_entries_[idx];
    }

    index_entries_state_.index_entries_ = new_buffer;
    index_entries_state_.index_entries_capacity_ = new_capacity;
  }

  auto &latest_index_entry = index_entries_state_.index_entries_[index_entries_state_.index_entries_size_];
  latest_index_entry.data_block_offset_ = offset;
  latest_index_entry.data_block_size_ = size;

  index_entries_state_.index_entries_size_++;

  current_block_state_.arena_.destroy();
  current_block_state_.data_block_body_ =
      static_cast<char *>(current_block_state_.arena_.allocate(kSSTableArenaCapacity, alignof(std::uint64_t)));
  current_block_state_.data_block_capacity_ = kSSTableArenaCapacity;
  current_block_state_.data_block_size_ = 0;
  current_block_state_.buffer_writer_ =
      core::buffer_writer::create(core::to_writable_span(current_block_state_.data_block_body_, kSSTableArenaCapacity));

  return {};
}

std::uint32_t sstable_writer::get_data_block_size() const noexcept { return current_block_state_.data_block_size_; }

std::expected<std::string_view, core::status> sstable_writer::get_index() noexcept {
  FR_VERIFY(index_entries_state_.index_entries_ != nullptr);
  FR_VERIFY(index_entries_state_.index_entries_size_ != 0ULL);
  FR_VERIFY(index_entries_state_.index_entries_capacity_ != 0ULL);
  // FR_VERIFY(index_entries_state_.entry_count_ != 0ULL); ---- entry_count_ not updated, check

  std::uint64_t index_image_size = 0;
  for (std::uint32_t idx{0}; idx < index_entries_state_.index_entries_size_; idx++) {
    auto &current_index_entry = index_entries_state_.index_entries_[idx];
    index_image_size += sizeof(std::uint64_t) + current_index_entry.smallest_key_.size() + sizeof(std::uint64_t) +
                        sizeof(std::uint64_t);
  }
  auto *index_image =
      static_cast<char *>(index_entries_state_.arena_.allocate(index_image_size, sizeof(std::uint64_t)));
  if (index_image == nullptr) {
    return core::unexpected(core::status_code::out_of_memory);
  }

  auto index_buffer_writer = core::buffer_writer::create(core::to_writable_span(index_image, index_image_size));
  for (std::uint32_t idx{0}; idx < index_entries_state_.index_entries_size_; idx++) {
    auto &current_index_entry = index_entries_state_.index_entries_[idx];

    auto error_or = index_buffer_writer.write_string(current_index_entry.smallest_key_)
                        .write_endian_integer(core::le_uint64_t{current_index_entry.data_block_offset_})
                        .write_endian_integer(core::le_uint64_t{current_index_entry.data_block_size_})
                        .error();
    if (error_or) {
      return core::unexpected(core::status_code::invalid_argument);
    }
  }

  return std::string_view{index_image, index_image_size};
}

std::expected<std::string_view, core::status> sstable_writer::get_footer(sstable_footer footer) noexcept {
  constexpr std::uint32_t footer_image_size = sizeof(sstable_footer);
  auto *footer_image = static_cast<char *>(gpa_arena_.allocate(footer_image_size, sizeof(std::uint64_t)));
  if (footer_image == nullptr) {
    return core::unexpected(core::status_code::out_of_memory);
  }

  auto footer_buffer_writter = core::buffer_writer::create(core::to_writable_span(footer_image, footer_image_size));
  auto error_or = footer_buffer_writter.write_endian_integer(core::le_uint32_t{footer.index_size_})
                      .write_endian_integer(core::le_uint32_t{footer.index_offset_})
                      .error();
  if (error_or) {
    return core::unexpected(core::status_code::invalid_argument);
  }

  return std::string_view{footer_image, footer_image_size};
}

}  // namespace frankie::storage
