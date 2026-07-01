#include <cstdio>
#include <optional>
#include <print>
#include <utility>

#include "core/fs.hpp"
#include "core/scratch_arena.hpp"
#include "core/status.hpp"
#include "engine/engine.hpp"
#include "storage/memtable.hpp"
#include "storage/sstable_writer.hpp"

namespace frankie::engine {

std::expected<engine, core::status> engine::create(core::config config) noexcept {
  storage::memtable memtable_active = storage::memtable::create(config.memtable_capacity_);
  std::uint64_t max_sequence_number = 0;
  auto reader = wal_reader::open(config.wal_path_);
  if (!reader) {
    // not_found = no WAL on disk yet (first start). eof = WAL exists but is
    // empty. Both are clean-start cases; anything else is a real failure that
    // must propagate so the caller doesn't silently lose existing entries.
    if (reader.error().code_ != core::status_code::not_found && reader.error().code_ != core::status_code::eof)
        [[unlikely]] {
      std::println("engine::create: failed to open WAL reader. path={}, status={}", config.wal_path_.c_str(),
                   core::to_cstring(reader.error().code_));
      return core::unexpected(reader.error());
    }
  } else {
    std::expected<wal_entry, core::status> entry{core::unexpected(core::status_code::eof)};
    while ((entry = reader->read())) {
      const auto &wal_entry = entry.value();

      switch (wal_entry.operation_) {
        case wal_operation::put:
          memtable_active.put(wal_entry.key_, wal_entry.value_, wal_entry.sequence_, wal_entry.tombstone_);
          break;
        case wal_operation::del:
          memtable_active.put(wal_entry.key_, "", wal_entry.sequence_, wal_entry.tombstone_);
          break;
        default:
          FR_VERIFY_MSG(false, "missing handler for WAL operation");
          break;
      }

      max_sequence_number = std::max(max_sequence_number, wal_entry.sequence_);
    }

    if (entry.error().code_ != core::status_code::eof) [[unlikely]] {
      std::println("engine::create: failed to recover WAL. path={}, status={}", config.wal_path_.c_str(),
                   core::to_cstring(entry.error().code_));
      return core::unexpected(entry.error());
    }
  }

  auto writer = wal_writer::open(config.wal_path_, config.wal_capacity_);
  if (!writer) {
    std::println("engine::create: failed to open WAL writer. path={}", config.wal_path_.c_str());
    return core::unexpected(writer.error());
  }

  engine result;
  result.config_ = std::move(config);
  result.sequence_ = max_sequence_number + 1;
  result.memtable_active_ = std::move(memtable_active);
  result.wal_ = std::move(writer.value());
  return result;
}

std::expected<void, core::status> engine::put(std::string_view key, std::string_view value) noexcept {
  const auto sequence = get_next_sequence();

  if (!wal_.append({
          .operation_ = wal_operation::put,
          .sequence_ = sequence,
          .key_ = key,
          .value_ = value,
          .tombstone_ = false,
      })) {
    return core::unexpected(core::status_code::io_error);
  }

  memtable_active_.put(key, value, sequence, false);
  if (auto status = maybe_rotate_memtable(); !status) {
    return status;
  }

  return {};
}

std::expected<std::string_view, core::status> engine::get(std::string_view key) noexcept {
  auto entry = memtable_active_.get(key);
  if (entry.has_value()) {
    if (entry.value().tombstone_) {
      return core::unexpected(core::status_code::not_found);
    }
    return entry.value().value();
  }

  if (memtable_immutable_.has_value()) {
    entry = memtable_immutable_->get(key);
    if (entry.has_value()) {
      if (entry.value().tombstone_) {
        return core::unexpected(core::status_code::not_found);
      }
      return entry.value().value();
    }
  }

  return core::unexpected(core::status_code::not_found);
}

std::expected<void, core::status> engine::del(std::string_view key) noexcept {
  const auto sequence = get_next_sequence();

  if (!wal_.append({
          .operation_ = wal_operation::del,
          .sequence_ = sequence,
          .key_ = key,
          .value_ = "",
          .tombstone_ = true,
      })) {
    return core::unexpected(core::status_code::io_error);
  }

  memtable_active_.put(key, {}, sequence, true);
  if (auto status = maybe_rotate_memtable(); !status) {
    return status;
  }

  return {};
}

// TODO(lnikon): Interface tdb
void engine::scan(std::string_view range_start_key, std::string_view range_end_key) noexcept {
  (void)range_start_key;
  (void)range_end_key;
}

std::expected<void, core::status> engine::maybe_rotate_memtable() noexcept {
  if (memtable_active_.bytes_allocated() <= memtable_active_.capacity()) {
    return {};
  }

  memtable_immutable_.emplace(std::move(memtable_active_));
  memtable_active_ = storage::memtable::create(memtable_active_.capacity());

  scratch_arena_.reset();

  auto formatted_sstable_path =
      std::span{scratch_arena_.allocate(kEngineScratchArenaCapacity), kEngineScratchArenaCapacity};

  const auto len = std::snprintf(formatted_sstable_path.data(), kEngineScratchArenaCapacity, "%s%lu",
                                 sstable_file_prefix_.data(), sstable_id_);
  if (len <= 0) {
    return core::unexpected(core::status_code::invalid_argument);
  }
  const auto formatted_len = static_cast<std::size_t>(len);

  auto sst_file = core::random_access_file::create_exclusive(
      config_.sstable_dir_path_ / std::string_view{formatted_sstable_path.data(), formatted_len});

  if (!sst_file) {
    return core::unexpected(sst_file.error());
  }

  auto writer = storage::sstable_writer::create(storage::sstable_writer_config{});
  if (!writer) {
    return core::unexpected(writer.error());
  }

  std::uint64_t data_block_body_offset = 0;
  for (auto [ikey, value] : *memtable_immutable_) {
    if (auto result = writer->append(ikey, value); !result) {
      return core::unexpected(result.error());
    }

    if (writer->is_data_block_complete()) {
      auto get_data_block_or = writer->get_data_block();
      if (!get_data_block_or) {
        return core::unexpected(get_data_block_or.error());
      }

      auto write_result = sst_file->write(get_data_block_or.value(), data_block_body_offset);
      if (!write_result) {
        return core::unexpected(write_result.error());
      }
      const std::uint64_t data_block_size = write_result.value();

      if (auto record_result = writer->record_data_block(data_block_body_offset, data_block_size); !record_result) {
        return core::unexpected(record_result.error());
      }

      data_block_body_offset += data_block_size;
    }
  }

  if (writer->get_data_block_size() != 0ULL) {
    auto get_data_block_or = writer->get_data_block();
    if (!get_data_block_or) {
      return core::unexpected(get_data_block_or.error());
    }

    auto write_result = sst_file->write(get_data_block_or.value(), data_block_body_offset);
    if (!write_result) {
      return core::unexpected(write_result.error());
    }
    const std::uint64_t data_block_size = write_result.value();

    if (auto record_result = writer->record_data_block(data_block_body_offset, data_block_size); !record_result) {
      return core::unexpected(record_result.error());
    }

    data_block_body_offset += data_block_size;
  }

  // Serialize index.
  auto index_or = writer->get_index();
  if (!index_or) {
    return core::unexpected(core::status_code::corrupted);
  }
  const auto index_write_result = sst_file->write(index_or.value(), data_block_body_offset);
  if (!index_write_result) {
    return core::unexpected(index_write_result.error());
  }
  const std::uint64_t index_size = index_write_result.value();
  // Index starts where the last data block ends.
  const std::uint64_t index_offset = data_block_body_offset;

  // Serialize footer.
  const std::uint64_t footer_offset = data_block_body_offset + index_size;
  auto footer_or = writer->get_footer({.index_offset_ = static_cast<std::uint32_t>(index_offset),
                                       .index_size_ = static_cast<std::uint32_t>(index_size)});
  if (!footer_or) {
    return core::unexpected(footer_or.error());
  }
  const auto footer_write_result = sst_file->write(footer_or.value(), footer_offset);
  if (!footer_write_result) {
    return core::unexpected(footer_write_result.error());
  }

  sstable_id_++;

  if (!wal_.truncate()) {
    // TODO(lnikon): Need a fallback strategy e.g. move current wal into wal.old1 and create a new wal
    return core::unexpected(core::status_code::io_error);
  }

  return {};
}

std::uint64_t engine::get_next_sequence() noexcept { return sequence_++; }

}  // namespace frankie::engine
