#pragma once

#include <expected>
#include <string_view>

#include "core/arena.hpp"
#include "core/fs.hpp"
#include "core/serialization/buffer_writer.hpp"
#include "core/status.hpp"
#include "storage/sstable_format.hpp"

namespace frankie::storage {

struct sstable_writer_options final {
  std::uint64_t target_block_size_{4096};  // bytes
  std::uint64_t max_entry_size{4096};      // bytes
  sstable_compression_type compression_{sstable_compression_type::none};
  std::uint64_t block_in_flight_{1};  // 1 - synchronous; 8 - io_uring
};

class sstable_writer final {
  core::random_access_file file_;

  core::arena arena_;

  std::span<std::byte> body_span_;
  core::buffer_writer body_writer_;

 public:
  sstable_writer() = default;
  sstable_writer(const sstable_writer &) = delete;
  sstable_writer &operator=(const sstable_writer &) = delete;
  sstable_writer(sstable_writer &&) = default;
  sstable_writer &operator=(sstable_writer &&) = default;
  ~sstable_writer() noexcept;

  [[nodiscard]] static std::expected<sstable_writer, core::status> create(core::random_access_file file, sstable_writer_options options) noexcept;

  [[nodiscard]] std::expected<void, core::status> add(std::string_view encoded_internal_key, std::string_view value) noexcept;

  [[nodiscard]] std::expected<void, core::status> finish() noexcept;
};

}  // namespace frankie::storage
