#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cassert>
#include <cstring>
#include <optional>
#include <print>
#include <utility>

#include "core/crc32.hpp"
#include "core/scratch_arena.hpp"
#include "engine/wal.hpp"

namespace frankie::engine {

// Wire format:
// record_len: u32
// crc32: u32
// operation: u8
// sequence: u64
// tombstone: u8
// key_len: u32
// value_len: u32
// key_bytes: u8[]
// value_bytes: u8[]
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

std::optional<wal_entry> wal_entry::decode(std::string_view encoded) noexcept {
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

  return result;
}

wal_writer::wal_writer(wal_writer &&other) noexcept
    : path_{std::exchange(other.path_, std::filesystem::path{})},
      fd_{std::exchange(other.fd_, -1)},
      capacity_{std::exchange(other.capacity_, 0)},
      scratch_arena_{std::move(other.scratch_arena_)} {}

wal_writer &wal_writer::operator=(wal_writer &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  if (fd_ != -1) {
    if (!close()) {
      std::println("wal: close failed during move-assign, fd={}, errno={}\n", fd_, strerror(errno));
    }
  }

  path_ = std::exchange(other.path_, std::filesystem::path{});
  fd_ = std::exchange(other.fd_, -1);
  capacity_ = std::exchange(other.capacity_, 0);
  scratch_arena_ = std::move(other.scratch_arena_);

  return *this;
}

wal_writer::~wal_writer() noexcept {
  if (fd_ != -1) {
    if (!close()) {
      std::println("wal: close failed during dtor, fd={}, errno={}\n", fd_, strerror(errno));
    }
  }
}

std::optional<wal_writer> wal_writer::open(std::filesystem::path path, std::uint64_t capacity) noexcept {
  const std::int32_t fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd == -1) {
    std::println("wal: failed to open file. path={}, fd={}, errno={}", path.c_str(), fd, strerror(errno));
    return std::nullopt;
  }

  // fsync() parent directory of the WAL to synchronize its metadata with its content
  const auto &parent_dir_path = path.parent_path();
  const std::int32_t parent_dir_fd = ::open(parent_dir_path.c_str(), O_RDONLY);
  if (parent_dir_fd == -1) {
    std::println("wal: failed to open parent dir. dir={}, fd={}, errno={}", parent_dir_path.c_str(), parent_dir_fd,
                 strerror(errno));
    return std::nullopt;
  }
  const int parent_rc = ::fsync(parent_dir_fd);
  ::close(parent_dir_fd);
  if (parent_rc != 0) {
    std::println("wal: fsync failed. dir={}, rc={}, errno={}", parent_dir_path.c_str(), parent_rc, strerror(errno));
    return std::nullopt;
  }

  std::optional<wal_writer> result;
  result.emplace();
  result->path_ = std::move(path);
  result->fd_ = fd;
  result->capacity_ = capacity;
  return result;
}

bool wal_writer::append(const wal_entry &entry) noexcept {
  assert(fd_ != -1);

  const auto encoded_entry = entry.encode(scratch_arena_);

  const ssize_t written = ::write(fd_, encoded_entry.data(), encoded_entry.size());
  if (written != static_cast<ssize_t>(encoded_entry.size())) {
    std::println("wal: write failed. written={}", written);
    return false;
  }

  const int rc = fdatasync(fd_);
  if (rc != 0) {
    std::println("wal: fdatasync failed. rc={}, errno={}", rc, strerror(errno));
    return false;
  }

  return true;
}

bool wal_writer::sync() noexcept {
  assert(fd_ != -1);

  const std::int32_t rc = fdatasync(fd_);
  if (rc != 0) {
    std::println("wal: fdatasync failed. rc={}, errno={}", rc, strerror(errno));
    return false;
  }

  return true;
}

bool wal_writer::truncate() noexcept {
  assert(fd_ != -1);

  if (fd_ == -1) {
    std::println("wal_writer::truncate: invalid file description. path={}, fd={}", path_.c_str(), fd_);
    return false;
  }

  const std::int32_t rc = ::ftruncate(fd_, 0);
  if (rc == -1) {
    std::println("wal_writer::truncate: failed to truncate wal. path={}, fd={}, errno={}", path_.c_str(), fd_,
                 strerror(errno));
    return false;
  }

  const off_t off = ::lseek(fd_, 0, SEEK_SET);
  if (off == -1) {
    std::println("wal_writer::truncate: failed to lseek wal. path={}, fd={}, errno={}", path_.c_str(), fd_,
                 strerror(errno));
    return false;
  }

  return true;
}

bool wal_writer::close() noexcept {
  assert(fd_ != -1);

  // close() is a good example of why dealing with filesystems is a pain in the butt.
  // It may fail, and has limited error codes to return.
  // Thing is, the Linux implementation of close() frees fd early in the implementation,
  // which means regardless of the close() failing, the fd is gone anyway.
  // This makes retrying close(fd) impossible.
  // A proper handling of close() boils down to two things (according to `man 2 close`):
  // 1. Precede close() with an fsync() or succeed write() with an fsync() (which we do in append()),
  // 2. Log on close() error.
  const std::int32_t rc = ::close(fd_);
  fd_ = -1;
  if (rc != 0) {
    std::println("wal: close failed. rc={}, errno={}", rc, strerror(errno));
    return false;
  }
  return true;
}

}  // namespace frankie::engine
