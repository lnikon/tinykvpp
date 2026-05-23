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
    result.m_buffer = buffer;
    return result;
  }

  template <EndianInteger T>
  [[nodiscard]] auto write_endian_integer(const T value) noexcept -> buffer_writer & {
    if (m_error) {
      return *this;
    }

    write_raw_bytes(value.bytes());

    return *this;
  }

  [[nodiscard]] auto write_varint(std::uint64_t value) noexcept -> buffer_writer & {
    if (m_error) {
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
    if (m_error) {
      return *this;
    }

    write_raw_bytes(buffer);

    return *this;
  }

  [[nodiscard]] auto write_string(const std::string_view str) noexcept -> buffer_writer & {
    if (m_error) {
      return *this;
    }

    (void)write_varint(str.size());
    // (void)write_endian_integer(le_uint64_t{static_cast<std::uint64_t>(str.size())});
    if (m_error) {
      return *this;
    }

    write_raw_bytes(to_span(str));

    return *this;
  }

  [[nodiscard]] auto bytes_written() const noexcept -> std::size_t { return m_cursor; }

  [[nodiscard]] auto error() const noexcept -> std::optional<serialization_error_k> { return m_error; }

  [[nodiscard]] auto has_error() const noexcept -> bool { return m_error.has_value(); }

  [[nodiscard]] auto set_cursor(std::uint64_t cursor) noexcept -> std::uint64_t {
    FR_VERIFY(cursor <= m_buffer.size());
    auto oldCursor = m_cursor;
    m_cursor = cursor;
    return oldCursor;
  }

  void set_buffer(std::span<std::byte> new_buffer) noexcept {
    FR_VERIFY(new_buffer.size() >= m_buffer.size());
    m_buffer = new_buffer;
  }

 private:
  void write_raw_bytes(std::span<const byte> bytes) noexcept {
    if (m_cursor + bytes.size() > m_buffer.size()) {
      std::println(
          "write_raw_bytes: Failed to serialize bytes due to buffer overflow. m_cursor={} + bytes.size()={}, "
          "m_buffer.size()={}",
          m_cursor, bytes.size(), m_buffer.size());
      m_error = serialization_error_k::buffer_overflow_k;
      return;
    }

    std::memcpy(&m_buffer[m_cursor], bytes.data(), bytes.size());

    m_cursor += bytes.size();
  }

  std::span<byte> m_buffer;
  std::uint64_t m_cursor{0};
  std::optional<serialization_error_k> m_error{std::nullopt};
};

}  // namespace frankie::core
