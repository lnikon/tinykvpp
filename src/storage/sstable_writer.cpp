#include "storage/sstable_writer.hpp"
#include <cstring>
#include "core/assert.hpp"
#include "core/status.hpp"
#include "core/views.hpp"
#include "storage/sstable_format.hpp"

namespace frankie::storage {

sstable_writer::~sstable_writer() noexcept { arena_.destroy(); }

std::expected<sstable_writer, core::status> sstable_writer::create(core::random_access_file file) noexcept {
  core::arena arena = core::arena::create(kBodyAllocationCapacityBytes);
  char *body_buffer = static_cast<char *>(arena.allocate(kBodyAllocationCapacityBytes, alignof(std::uint8_t)));
  if (body_buffer == nullptr) {
    return core::unexpected(core::status_code::out_of_memory);
  }

  sstable_writer result;
  result.file_ = std::move(file);
  result.arena_ = std::move(arena);
  // Size of the span acts as a capacity.
  result.body_span_ = core::to_writable_span(body_buffer, kBodyAllocationCapacityBytes);
  // How many bytes we have written into the body so far.
  result.body_offset_ = 0;
  // Holding a pointer to body_span_ so that we do not have to re-create the writer after growing the body.
  result.body_writer_ = core::buffer_writer::create(&result.body_span_);
  return result;
}

std::expected<void, core::status> sstable_writer::add(std::string_view ikey, std::string_view value) noexcept {
  FR_VERIFY(!body_span_.empty());

  // TODO(lnikon): What to do if entry size is 0?
  const auto entry_size = sstable_data_block::calculate_entry_size(ikey, value);
  if (entry_size + body_offset_ >= body_span_.size()) {
    const auto body_buffer_size = body_span_.size() * 2;
    char *body_buffer = static_cast<char *>(arena_.allocate(body_buffer_size, alignof(std::uint8_t)));
    if (body_buffer == nullptr) {
      return core::unexpected(core::status_code::out_of_memory);
    }

    std::memcpy(body_buffer, body_span_.data(), body_span_.size());
    body_span_ = core::to_writable_span(body_buffer, body_buffer_size);
  }

  if (const auto result =
          body_writer_.write(ikey.size()).write_bytes(core::to_span(ikey)).write(value.size()).write_bytes(core::to_span(value)).error();
      !result) {
    return core::unexpected(result.error());
  }
  body_offset_ += entry_size;

  return {};
}

[[nodiscard]] std::expected<void, core::status> sstable_writer::finish() noexcept { return core::unexpected(core::status_code::invalid_argument); }

}  // namespace frankie::storage
