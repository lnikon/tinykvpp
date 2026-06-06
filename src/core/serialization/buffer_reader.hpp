#pragma once

#include <spdlog/spdlog.h>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "core/serialization/common.hpp"
#include "core/serialization/concepts.hpp"

namespace frankie::core {

class buffer_reader {
 public:
  buffer_reader(std::span<std::byte> buffer) : buffer_{buffer} {}

  template <EndianInteger T>
  [[nodiscard]] auto read_endian_integer(T &out) noexcept -> buffer_reader & {
    if (error_) {
      return *this;
    }

    auto bytes{read_raw_bytes(T::SIZE)};
    if (error_) {
      return *this;
    }
    out = T::from_bytes(bytes);

    return *this;
  }

  [[nodiscard]] auto read_varint(std::uint64_t &out) noexcept -> buffer_reader & {
    if (error_) {
      return *this;
    }

    std::uint64_t result{0};
    std::size_t shift{0};
    std::size_t byteCount{0};

    while (byteCount < MAX_VARINT_BYTES) {
      const auto bytes{read_raw_bytes(1)};
      if (has_error()) {
        return *this;
      }

      result |= (std::to_integer<std::uint64_t>((bytes[0] & std::byte{0x7F})) << shift);

      if ((bytes[0] & std::byte{0x80}) == std::byte{0}) {
        out = result;
        return *this;
      }

      shift += 7;
      byteCount++;
    }

    error_ = serialization_error_k::invalid_variant_k;
    return *this;
  }

  [[nodiscard]] auto read_string(std::string &out) noexcept -> buffer_reader & {
    if (error_) {
      return *this;
    }

    std::size_t count{0};
    (void)read_varint(count);
    if (error_) {
      return *this;
    }

    auto span{read_raw_bytes(count)};
    if (has_error()) {
      return *this;
    }
    out = to_string_view(span);

    return *this;
  }

  [[nodiscard]] auto read_bytes(const std::size_t count, std::span<std::byte> &out) noexcept -> buffer_reader & {
    if (has_error()) {
      return *this;
    }

    out = read_raw_bytes(count);

    return *this;
  }

  [[nodiscard]] std::size_t bytes_read() const noexcept { return position_; }

  [[nodiscard]] std::size_t remaining() const noexcept { return buffer_.size() - position_; }

  [[nodiscard]] std::optional<serialization_error_k> error() const noexcept { return error_; }

  [[nodiscard]] bool has_error() const noexcept { return error_.has_value(); }

  [[nodiscard]] bool eof() const noexcept { return position_ == buffer_.size(); }

 private:
  auto read_raw_bytes(std::size_t count) noexcept -> std::span<std::byte> {
    if (count + position_ > buffer_.size()) {
      error_ = serialization_error_k::truncated_file_k;
      return {};
    }

    auto result{buffer_.subspan(position_, count)};
    position_ += count;
    return result;
  }

  std::span<std::byte> buffer_;
  std::size_t position_{0};
  std::optional<serialization_error_k> error_{std::nullopt};
};

}  // namespace frankie::core
