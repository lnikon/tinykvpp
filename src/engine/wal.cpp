#include <cstdint>
#include <cstring>
#include <optional>
#include <print>
#include <utility>

#include "core/crc32.hpp"
#include "core/fs.hpp"
#include "core/scratch_arena.hpp"
#include "engine/wal.hpp"

namespace frankie::engine {

// =============================================================================
// wal_entry
// Wire format: record_len: u32 | crc32: u32   | operation: u8  | sequence: u64
//              tombstone: u8   | key_len: u32 | value_len: u32 | key_bytes: u8[] | value_bytes: u8[]
// =============================================================================
std::string_view wal_entry::encode(core::scratch_arena &arena) const noexcept {
  arena.reset();

  const auto key_size = static_cast<std::uint32_t>(key_.size());
  const auto value_size = static_cast<std::uint32_t>(value_.size());

  const std::uint32_t buffer_size = kMetadataSize + key_size + value_size;
  auto *buf = arena.allocate(buffer_size);
  std::memset(buf, 0, buffer_size);

  // record_len counts evertyhing that comes after record_len(u32) + crc32(u32) fields.
  const std::uint32_t record_len = buffer_size - static_cast<std::uint32_t>(sizeof(std::uint32_t)) -
                                   static_cast<std::uint32_t>(sizeof(std::uint32_t));
  // The actual record starts after record_len(u32) + crc32(u32) fields.
  const std::uint32_t record_offset = sizeof(std::uint32_t) + sizeof(std::uint32_t);

  auto *cursor = buf;
  std::memcpy(cursor, &record_len, sizeof(record_len));
  cursor += sizeof(record_len);
  cursor += sizeof(std::uint32_t);  // Skip CRC32 checksum. It is calculated when buffer is complete.
  std::memcpy(cursor, &operation_, sizeof(operation_));
  cursor += sizeof(operation_);
  std::memcpy(cursor, &sequence_, sizeof(sequence_));
  cursor += sizeof(sequence_);
  std::memcpy(cursor, &tombstone_, sizeof(tombstone_));
  cursor += sizeof(tombstone_);
  std::memcpy(cursor, &key_size, sizeof(key_size));
  cursor += sizeof(key_size);
  std::memcpy(cursor, &value_size, sizeof(value_size));
  cursor += sizeof(value_size);
  std::memcpy(cursor, key_.data(), key_size);
  cursor += key_size;
  std::memcpy(cursor, value_.data(), value_size);
  cursor += value_size;

  const std::uint32_t crc32_checksum =
      core::crc32{}
          .update({reinterpret_cast<const std::byte *>(buf) + record_offset, buffer_size - record_offset})
          .finalize();
  std::memcpy(buf + sizeof(record_len), &crc32_checksum, sizeof(crc32_checksum));

  return {buf, buffer_size};
}

std::optional<wal_entry> wal_entry::decode(std::string_view &encoded) noexcept {
  if (encoded.size() < kMetadataSize) {
    return std::nullopt;
  }

  const auto *cursor = encoded.data();

  std::uint32_t record_len{};
  std::memcpy(&record_len, cursor, sizeof(record_len));
  cursor += sizeof(record_len);

  std::uint32_t stored_crc32{};
  std::memcpy(&stored_crc32, cursor, sizeof(stored_crc32));
  cursor += sizeof(stored_crc32);

  const std::uint32_t record_offset = sizeof(record_len) + sizeof(stored_crc32);
  if (record_len + record_offset > encoded.size()) {
    std::println("wal_entry::decode: not enough bytes to encode. needed={}, given={}", record_len + record_offset,
                 encoded.size());
    return std::nullopt;
  }

  const std::uint32_t computed_crc32 =
      core::crc32{}
          .update({reinterpret_cast<const std::byte *>(encoded.data()) + record_offset, record_len})
          .finalize();
  if (stored_crc32 != computed_crc32) {
    std::println("wal_entry::decode: crc32 mismatch. stored={}, computed={}", stored_crc32, computed_crc32);
    return std::nullopt;
  }

  std::optional<wal_entry> result;
  result.emplace();

  std::memcpy(&result->operation_, cursor, sizeof(result->operation_));
  cursor += sizeof(result->operation_);

  std::memcpy(&result->sequence_, cursor, sizeof(result->sequence_));
  cursor += sizeof(result->sequence_);

  std::memcpy(&result->tombstone_, cursor, sizeof(result->tombstone_));
  cursor += sizeof(result->tombstone_);

  std::uint32_t key_len{};
  std::memcpy(&key_len, cursor, sizeof(key_len));
  cursor += sizeof(key_len);

  std::uint32_t value_len{};
  std::memcpy(&value_len, cursor, sizeof(value_len));
  cursor += sizeof(value_len);

  if (const char *payload_end = encoded.data() + record_offset + record_len;
      cursor + key_len + value_len > payload_end) {
    return std::nullopt;
  }

  result->key_ = {cursor, key_len};
  cursor += key_len;
  result->value_ = {cursor, value_len};

  encoded.remove_prefix(record_offset + record_len);

  return result;
}

// =============================================================================
// wal_writer
// =============================================================================
wal_writer::wal_writer(wal_writer &&other) noexcept
    : file_{std::exchange(other.file_, {})},
      capacity_{std::exchange(other.capacity_, 0)},
      scratch_arena_{std::move(other.scratch_arena_)} {}

wal_writer &wal_writer::operator=(wal_writer &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  (void)file_.close();

  file_ = std::exchange(other.file_, {});
  capacity_ = std::exchange(other.capacity_, 0);
  scratch_arena_ = std::move(other.scratch_arena_);

  return *this;
}

wal_writer::~wal_writer() noexcept = default;

std::optional<wal_writer> wal_writer::open(std::filesystem::path path, std::uint64_t capacity) noexcept {
  auto file = core::append_only_file::open(std::move(path));
  if (!file) {
    return std::nullopt;
  }

  std::optional<wal_writer> result;
  result.emplace();
  result->file_ = std::move(file.value());
  result->capacity_ = capacity;
  return result;
}

bool wal_writer::append(const wal_entry &entry) noexcept {
  const auto encoded_entry = entry.encode(scratch_arena_);
  return file_.append_fsync(encoded_entry).has_value();
}

bool wal_writer::sync() noexcept { return file_.sync().has_value(); }

bool wal_writer::truncate() noexcept { return file_.truncate().has_value(); }

bool wal_writer::close() noexcept { return file_.close().has_value(); }

// =============================================================================
// wal_reader
// =============================================================================
wal_reader::wal_reader(wal_reader &&other) noexcept
    : file_{std::move(other.file_)},
      scratch_arena_{std::move(other.scratch_arena_)},
      wal_view_{std::exchange(other.wal_view_, {})} {}

wal_reader &wal_reader::operator=(wal_reader &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  (void)file_.close();

  file_ = std::move(other.file_);
  scratch_arena_ = std::move(other.scratch_arena_);
  wal_view_ = std::exchange(other.wal_view_, {});

  return *this;
}

wal_reader::~wal_reader() noexcept = default;

std::optional<wal_reader> wal_reader::open(std::filesystem::path path) noexcept {
  auto file = core::random_access_file::open_read(std::move(path));
  if (!file) {
    return std::nullopt;
  }

  auto file_size = file->size();
  if (!file_size) {
    return std::nullopt;
  }
  if (file_size.value() == 0) {
    // Empty WAL — nothing to recover. Caller proceeds as if no log existed.
    return std::nullopt;
  }

  core::scratch_arena scratch_arena;
  char *buf = scratch_arena.allocate(file_size.value());

  // Slurp the entire file. Loop on short reads — pread on a regular file only
  // returns short at EOF, which here means the file shrank between size() and
  // read() (a race or truncation): treat as recovery failure.
  std::uint64_t total = 0;
  while (total < file_size.value()) {
    auto chunk = file->read({buf + total, file_size.value() - total}, total);
    if (!chunk) {
      return std::nullopt;
    }
    total += chunk.value();
  }

  std::optional<wal_reader> result;
  result.emplace();
  result->file_ = std::move(file.value());
  result->scratch_arena_ = std::move(scratch_arena);
  result->wal_view_ = {buf, file_size.value()};
  return result;
}

[[nodiscard]] std::optional<wal_entry> wal_reader::read() noexcept { return wal_entry::decode(wal_view_); }

[[nodiscard]] bool wal_reader::close() noexcept { return file_.close().has_value(); }

}  // namespace frankie::engine
