#pragma once

#include <cstdint>
#include <optional>

#include "core/serialization/codec.hpp"
#include "core/status.hpp"
#include "core/views.hpp"

namespace frankie::core {

class buffer_reader final {
  std::span<const std::byte> buffer_;
  std::size_t cursor_{0};
  std::optional<core::status_code> error_{std::nullopt};

 public:
  explicit buffer_reader(std::span<const std::byte> buffer) : buffer_{buffer} {}

  template <std::integral T>
  [[nodiscard]] auto read(T &out) noexcept -> buffer_reader & {
    if (error_) {
      return *this;
    }
    out = codec::load_le<T>(buffer_.subspan(cursor_).first<sizeof(T)>());
    cursor_ += sizeof(T);
    return *this;
  }

  [[nodiscard]] auto read_varint(std::uint64_t &out) noexcept -> buffer_reader & {
    if (error_) {
      return *this;
    }
    if (auto value = codec::decode_varint(buffer_, cursor_); value) {
      out = *value;
    } else {
      error_ = core::status_code::corrupted;
    }
    return *this;
  }

  [[nodiscard]] auto read_string_view(std::string_view &out) noexcept -> buffer_reader & {
    if (error_) {
      return *this;
    }
    std::uint64_t count = 0;
    std::span<const std::byte> bytes;
    (void)read_varint(count).read_bytes(count, bytes);
    if (error_) {
      return *this;
    }
    out = to_string_view(bytes);
    return *this;
  }

  [[nodiscard]] auto read_bytes(const std::size_t count, std::span<const std::byte> &out) noexcept -> buffer_reader & {
    if (has_error()) {
      return *this;
    }
    if (count + cursor_ > buffer_.size()) {
      error_ = core::status_code::buffer_overflow;
    }
    out = buffer_.subspan(cursor_, count);
    cursor_ += count;
    return *this;
  }

  [[nodiscard]] std::size_t bytes_read() const noexcept { return cursor_; }
  [[nodiscard]] std::size_t remaining() const noexcept { return buffer_.size() - cursor_; }

  [[nodiscard]] std::optional<core::status_code> error() const noexcept { return error_; }
  [[nodiscard]] bool has_error() const noexcept { return error_.has_value(); }
  [[nodiscard]] bool eof() const noexcept { return cursor_ == buffer_.size(); }
};

}  // namespace frankie::core
