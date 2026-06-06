#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>

#include "core/assert.hpp"
#include "core/serialization/common.hpp"
#include "core/serialization/concepts.hpp"
#include "core/serialization/endian_integer.hpp"

namespace frankie::core {

using byte = std::byte;

class buffer_writer final {
 public:
  buffer_writer() = default;
  buffer_writer(const buffer_writer &) = delete;
  buffer_writer &operator=(const buffer_writer &) = delete;
  buffer_writer(buffer_writer &&) = default;
  buffer_writer &operator=(buffer_writer &&) = default;
  ~buffer_writer() = default;

  [[nodiscard]] static buffer_writer create(std::span<byte> buffer) noexcept {
    FR_VERIFY(buffer.size() > 0ULL);

    buffer_writer result;
    result.buffer_ = buffer;
    return result;
  }

  template <EndianInteger T>
  [[nodiscard]] auto write_endian_integer(const T value) noexcept -> buffer_writer & {
    if (error_) {
      return *this;
    }

    write_raw_bytes(value.bytes());

    return *this;
  }

  [[nodiscard]] auto write_varint(std::uint64_t value) noexcept -> buffer_writer & {
    if (error_) {
      return *this;
    }

    std::array<byte, MAX_VARINT_BYTES> scratch{};
    std::size_t count{0};

    while (value >= 128) {
      scratch[count++] = byte{static_cast<std::uint8_t>((value & 0x7F))} | byte{0x80};
      value >>= 7;
    }
    scratch[count++] = byte{static_cast<std::uint8_t>((value & 0x7F))};

    write_raw_bytes(std::span(scratch.data(), count));

    return *this;
  }

  [[nodiscard]] auto write_bytes(std::span<const byte> buffer) noexcept -> buffer_writer & {
    if (error_) {
      return *this;
    }

    write_raw_bytes(buffer);

    return *this;
  }

  [[nodiscard]] auto write_string(const std::string_view str) noexcept -> buffer_writer & {
    if (error_) {
      return *this;
    }

    (void)write_varint(str.size());
    // (void)write_endian_integer(le_uint64_t{static_cast<std::uint64_t>(str.size())});
    if (error_) {
      return *this;
    }

    write_raw_bytes(to_span(str));

    return *this;
  }

  [[nodiscard]] auto bytes_written() const noexcept -> std::size_t { return cursor_; }

  [[nodiscard]] auto error() const noexcept -> std::optional<serialization_error_k> { return error_; }

  [[nodiscard]] auto has_error() const noexcept -> bool { return error_.has_value(); }

  [[nodiscard]] auto set_cursor(std::uint64_t cursor) noexcept -> std::uint64_t {
    FR_VERIFY(cursor <= buffer_.size());
    auto oldCursor = cursor_;
    cursor_ = cursor;
    return oldCursor;
  }

  void set_buffer(std::span<std::byte> new_buffer) noexcept {
    FR_VERIFY(new_buffer.size() >= buffer_.size());
    buffer_ = new_buffer;
  }

 private:
  void write_raw_bytes(std::span<const byte> bytes) noexcept {
    if (cursor_ + bytes.size() > buffer_.size()) {
      std::println(
          "write_raw_bytes: Failed to serialize bytes due to buffer overflow. m_cursor={} + bytes.size()={}, "
          "m_buffer.size()={}",
          cursor_, bytes.size(), buffer_.size());
      error_ = serialization_error_k::buffer_overflow_k;
      return;
    }

    std::memcpy(&buffer_[cursor_], bytes.data(), bytes.size());

    cursor_ += bytes.size();
  }

  std::span<byte> buffer_;
  std::uint64_t cursor_{0};
  std::optional<serialization_error_k> error_{std::nullopt};
};

}  // namespace frankie::core
