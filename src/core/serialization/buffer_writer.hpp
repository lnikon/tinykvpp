#pragma once

#include <cstdint>
#include <cstring>
#include <optional>

#include "core/assert.hpp"
#include "core/serialization/codec.hpp"
#include "core/status.hpp"
#include "core/views.hpp"

namespace frankie::core {

using byte = std::byte;

class buffer_writer final {
  // Mutable view into the target buffer.
  std::span<byte> buffer_;
  // Last modification position.
  std::uint64_t cursor_{0};
  // Stateful error.
  std::optional<core::status_code> error_{std::nullopt};

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

  template <std::integral T>
  [[nodiscard]] auto write(const T value) noexcept -> buffer_writer & {
    if (error_) {
      return *this;
    }
    if (cursor_ + sizeof(T) > buffer_.size()) {
      error_ = core::status_code::buffer_overflow;
      return *this;
    }
    codec::store_le(value, buffer_.subspan(cursor_).first<sizeof(T)>());
    cursor_ += sizeof(T);
    return *this;
  }

  [[nodiscard]] auto write_varint(std::uint64_t value) noexcept -> buffer_writer & {
    if (error_) {
      return *this;
    }
    std::array<byte, kMaxVarintBytes> scratch{};
    return write_bytes({scratch.data(), codec::encode_varint(value, scratch)});
  }

  [[nodiscard]] auto write_bytes(std::span<const byte> bytes) noexcept -> buffer_writer & {
    if (error_) {
      return *this;
    }
    if (cursor_ + bytes.size() > buffer_.size()) {
      error_ = core::status_code::buffer_overflow;
      return *this;
    }
    std::memcpy(&buffer_[cursor_], bytes.data(), bytes.size());
    cursor_ += bytes.size();
    return *this;
  }

  [[nodiscard]] auto write_string(const std::string_view str) noexcept -> buffer_writer & {
    return write_varint(str.size()).write_bytes(to_span(str));
  }

  [[nodiscard]] auto bytes_written() const noexcept -> std::size_t { return cursor_; }

  [[nodiscard]] auto error() const noexcept -> std::optional<core::status_code> { return error_; }

  void set_buffer(std::span<std::byte> new_buffer) noexcept {
    FR_VERIFY(new_buffer.size() >= buffer_.size());
    buffer_ = new_buffer;
  }
};

}  // namespace frankie::core
