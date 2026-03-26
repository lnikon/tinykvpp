#include <cstring>
#include <fstream>
#include <optional>
#include <print>
#include "core/scratch_arena.hpp"

#include "engine/wal.hpp"

namespace frankie::engine {

wal_entry *wal_entry::create(core::arena &arena, const wal_operation operation, const std::uint64_t sequence,
                             const std::uint8_t tombstone, const std::string_view key,
                             const std::string_view value) noexcept {
  const auto size = sizeof(wal_entry) + key.size() + value.size();
  void *mem = arena.allocate(size, alignof(wal_entry));
  std::memset(mem, 0, size);

  auto *entry = ::new (mem) wal_entry{};
  entry->operation_ = operation;
  entry->sequence_ = sequence;
  entry->tombstone_ = tombstone;
  entry->key_size_ = key.size();
  entry->value_size_ = value.size();

  std::memcpy(entry->key_bytes().data(), key.data(), key.size());
  std::memcpy(entry->value_bytes().data(), value.data(), value.size());

  return entry;
}

std::string_view wal_entry::encode(core::scratch_arena &arena) const noexcept {
  arena.reset();

  const auto size = kMetadataSize + key_size_ + value_size_;
  auto *buf = arena.allocate(size);

  auto *cursor = buf;
  std::memcpy(cursor, &operation_, sizeof(operation_));
  cursor += sizeof(operation_);
  std::memcpy(cursor, &sequence_, sizeof(sequence_));
  cursor += sizeof(sequence_);
  std::memcpy(cursor, &tombstone_, sizeof(tombstone_));
  cursor += sizeof(tombstone_);
  std::memcpy(cursor, &key_size_, sizeof(key_size_));
  cursor += sizeof(key_size_);
  std::memcpy(cursor, &value_size_, sizeof(value_size_));
  cursor += sizeof(value_size_);
  std::memcpy(cursor, key().data(), key().size());
  cursor += key_size_;
  std::memcpy(cursor, value().data(), value().size());
  cursor += value_size_;

  return {buf, size};
}

[[nodiscard]] std::string_view wal_entry::key() const noexcept {
  auto *base = reinterpret_cast<const char *>(this) + sizeof(wal_entry);
  return {base, key_size_};
}

[[nodiscard]] std::string_view wal_entry::value() const noexcept {
  auto *base = reinterpret_cast<const char *>(this) + sizeof(wal_entry) + key_size_;
  return {base, value_size_};
}

[[nodiscard]] std::span<std::byte> wal_entry::key_bytes() noexcept {
  auto *base = reinterpret_cast<std::byte *>(this) + sizeof(wal_entry);
  return {base, key_size_};
}

[[nodiscard]] std::span<const std::byte> wal_entry::key_bytes() const noexcept {
  auto *base = reinterpret_cast<const std::byte *>(this) + sizeof(wal_entry);
  return {base, key_size_};
}

[[nodiscard]] std::span<std::byte> wal_entry::value_bytes() noexcept {
  auto *base = reinterpret_cast<std::byte *>(this) + sizeof(wal_entry) + key_size_;
  return {base, value_size_};
}

[[nodiscard]] std::span<const std::byte> wal_entry::value_bytes() const noexcept {
  auto *base = reinterpret_cast<const std::byte *>(this) + sizeof(wal_entry) + key_size_;
  return {base, value_size_};
}

std::optional<wal_writer> wal_writer::open(std::filesystem::path path, std::uint64_t capacity) noexcept {
  wal_writer wal_writer;
  wal_writer.path_ = std::move(path);
  wal_writer.file_ = std::fstream(path);
  if (!wal_writer.file_.is_open()) {
    std::print("wal: failed to open wal. path={}", path.c_str());
    return std::nullopt;
  }

  wal_writer.arena_ = core::arena::create(capacity);
  wal_writer.capacity_ = capacity;
  // TODO(lnikon): Need to init scratch_arena? Maybe revisit its API

  return wal_writer;
}

bool wal_writer::append(wal_operation operation, std::uint64_t sequence, std::uint8_t tombstone, std::string_view key,
                        std::string_view value) noexcept {
  auto *entry = wal_entry::create(arena_, operation, sequence, tombstone, key, value);

  auto encoded_entry = entry->encode(scratch_arena_);
  file_ << encoded_entry;
  const auto sync_status = file_.sync();

  return sync_status == 0;
}

bool wal_writer::sync() noexcept {
  if (!file_.is_open()) {
    return false;
  }
  const auto sync_status = file_.sync();
  return sync_status == 0;
}

void wal_writer::close() noexcept {
  if (!file_.is_open()) {
    return;
  }
  file_.close();
}

}  // namespace frankie::engine
