#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>

#include "serialization/common.h"
#include "serialization/concepts.h"

namespace serialization
{

class buffer_writer_t final
{
  public:
    buffer_writer_t(std::span<std::byte> buffer)
        : m_buffer{buffer}
    {
    }

    template <EndianInteger T>
    [[nodiscard]] auto write_endian_integer(const T value) noexcept -> buffer_writer_t &
    {
        if (m_error)
        {
            return *this;
        }

        write_raw_bytes(value.bytes());

        return *this;
    }

    [[nodiscard]] auto write_varint(std::uint64_t value) noexcept -> buffer_writer_t &
    {
        if (m_error)
        {
            return *this;
        }

        std::array<std::byte, MAX_VARINT_BYTES> scratch{};
        std::size_t                             count{0};

        while (value > 127)
        {
            scratch[count++] =
                std::byte{static_cast<std::uint8_t>((value & 0x7F))} | std::byte{0x80};
            value >>= 7;
        }
        scratch[count++] = std::byte{static_cast<std::uint8_t>((value & 0x7F))};

        write_raw_bytes(std::span(scratch.data(), count));

        return *this;
    }

    [[nodiscard]] auto write_bytes(std::span<const std::byte> buffer) noexcept -> buffer_writer_t &
    {
        if (m_error)
        {
            return *this;
        }

        write_raw_bytes(buffer);

        return *this;
    }

    [[nodiscard]] auto write_string(const std::string_view str) noexcept -> buffer_writer_t &
    {
        if (m_error)
        {
            return *this;
        }

        (void)write_varint(str.size());
        if (m_error)
        {
            return *this;
        }

        write_raw_bytes(to_span(str));

        return *this;
    }

    [[nodiscard]] auto bytes_written() const noexcept -> std::size_t
    {
        return m_position;
    }

    [[nodiscard]] auto error() const noexcept -> std::optional<serialization_error_k>
    {
        return m_error;
    }

    [[nodiscard]] auto has_error() const noexcept -> bool
    {
        return m_error.has_value();
    }

  private:
    void write_raw_bytes(std::span<const std::byte> bytes) noexcept
    {
        if (m_position + bytes.size() > m_buffer.size())
        {
            m_error = serialization_error_k::buffer_overflow_k;
            return;
        }

        std::memcpy(&m_buffer[m_position], bytes.data(), bytes.size());

        m_position += bytes.size();
    }

    std::span<std::byte>                 m_buffer;
    std::size_t                          m_position{0};
    std::optional<serialization_error_k> m_error{std::nullopt};
};

} // namespace serialization
