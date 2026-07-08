#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

#include "core/fs.hpp"
#include "core/status.hpp"
#include "storage/memtable.hpp"
#include "storage/sstable_format.hpp"

namespace frankie::storage {

inline constexpr std::uint32_t kSSTableArenaCapacity = 1024 * 1024;
// TODO(lnikon): Should be replaced with sizeof(segment_footer) when bloom and crc32 are added.
inline constexpr std::uint32_t kFooterOffsetFromEnd = 2 * sizeof(std::uint32_t);

struct sstable_reader_config final {};

class sstable_reader final {
 public:
  sstable_reader() = default;
  sstable_reader(const sstable_reader &) = delete;
  sstable_reader &operator=(const sstable_reader &) = delete;
  sstable_reader(sstable_reader &&) = default;
  sstable_reader &operator=(sstable_reader &&) = default;
  ~sstable_reader() noexcept;

  [[nodiscard]] static std::expected<sstable_reader, core::status> create(core::random_access_file file) noexcept;

  [[nodiscard]] std::expected<sstable_footer, core::status> read_footer() noexcept;

  [[nodiscard]] std::expected<std::span<index_entry>, core::status> read_index(core::arena &arena,
                                                                               const sstable_footer &footer) noexcept;

  [[nodiscard]] std::expected<sstable_data_block, core::status> read_data_block(core::arena &arena,
                                                                                const index_entry index) noexcept;

 private:
  core::random_access_file file_;
};

}  // namespace frankie::storage
