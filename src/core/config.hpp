#pragma once

#include <filesystem>

namespace frankie::core {

static constexpr std::uint64_t kDefaultMemtableCapacity = 64ULL * 1024 * 1024;  // 64MB
static constexpr std::uint64_t kDefaultWalCapacity = 64ULL * 1024 * 1024;       // 64MB

struct config final {
  // Directory which stores WAL, SSTables, etc...
  std::filesystem::path root_dir_path_{"."};
  // WAL path relative to root_dir_path.
  std::filesystem::path wal_path_{root_dir_path_ / "wal"};
  // SSTables directory path relative to root_dir_path.
  std::filesystem::path sstable_dir_path_{root_dir_path_ / "segments"};

  std::uint64_t wal_capacity_{kDefaultWalCapacity};
  std::uint64_t memtable_capacity_{kDefaultMemtableCapacity};
};

}  // namespace frankie::core
