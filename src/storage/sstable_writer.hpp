#pragma once

#include <expected>
#include <string_view>

#include "core/arena.hpp"
#include "core/fs.hpp"
#include "core/serialization/buffer_writer.hpp"
#include "core/status.hpp"

namespace frankie::storage {

class sstable_writer final {
  static constexpr std::uint64_t kBodyAllocationCapacityBytes = 4096;

  core::random_access_file file_;

  core::arena arena_;

  std::span<std::byte> body_span_;
  std::uint64_t body_offset_{0};
  core::buffer_writer body_writer_;

 public:
  sstable_writer() = default;
  sstable_writer(const sstable_writer &) = delete;
  sstable_writer &operator=(const sstable_writer &) = delete;
  sstable_writer(sstable_writer &&) = default;
  sstable_writer &operator=(sstable_writer &&) = default;
  ~sstable_writer() noexcept;

  [[nodiscard]] static std::expected<sstable_writer, core::status> create(core::random_access_file file) noexcept;

  [[nodiscard]] std::expected<void, core::status> add(std::string_view ikey, std::string_view value) noexcept;

  [[nodiscard]] std::expected<void, core::status> finish() noexcept;
};

}  // namespace frankie::storage
