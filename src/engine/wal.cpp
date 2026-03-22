#include "wal.hpp"

namespace frankie::engine {

wal_writer wal_writer::open(std::filesystem::path path, std::uint64_t capacity) noexcept {
  wal_writer wal_writer;
  wal_writer.path_ = std::move(path);
  wal_writer.arena_ = core::arena::create(capacity);
  wal_writer.capacity_ = capacity;
  // TODO(lnikon): Need to init scratch_arena? Maybe revisit its API
  return wal_writer;
}

}  // namespace frankie::engine
