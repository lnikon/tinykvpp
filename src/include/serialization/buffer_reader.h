#pragma once

#include <optional>
#include <span>
#include <cstdint>
#include <spdlog/spdlog.h>
#include <string>

#include "serialization/common.h"
#include "serialization/concepts.h"

namespace serialization
{

class buffer_reader_t
{
  public:
    buffer_reader_t(std::span<std::byte> buffer)
        : m_buffer{buffer}
    {
    }

    template <EndianInteger T>
    [[nodiscard]] auto read_endian_integer(T &out) noexcept -> buffer_reader_t &
    {
        if (m_error)
        {
            return *this;
        }

        auto bytes{read_raw_bytes(T::SIZE)};
        if (m_error)
        {
            return *this;
        }
        out = T::from_bytes(bytes);

        return *this;
    }

    [[nodiscard]] auto read_varint(std::uint64_t &out) noexcept -> buffer_reader_t &
    {
        if (m_error)
        {
            return *this;
        }

        std::uint64_t result{0};
        std::size_t   shift{0};
        std::size_t   byteCount{0};

        while (byteCount < MAX_VARINT_BYTES)
        {
            const auto bytes{read_raw_bytes(1)};
            if (has_error())
            {
                return *this;
            }

            result |= (std::to_integer<std::uint64_t>((bytes[0] & std::byte{0x7F})) << shift);

            if ((bytes[0] & std::byte{0x80}) == std::byte{0})
            {
                out = result;
                return *this;
            }

            shift += 7;
            byteCount++;
        }

        m_error = serialization_error_k::invalid_variant_k;
        return *this;
    }

    [[nodiscard]] auto read_string(std::string &out) noexcept -> buffer_reader_t &
    {
        if (m_error)
        {
            return *this;
        }

        std::uint64_t count{0};
        (void)read_varint(count);
        if (m_error)
        {
            return *this;
        }

        auto span{read_raw_bytes(count)};
        if (has_error())
        {
            return *this;
        }
        out = to_string_view(span);

        return *this;
    }

    [[nodiscard]] auto read_bytes(const std::size_t count, std::span<std::byte> &out) noexcept
        -> buffer_reader_t &
    {
        if (has_error())
        {
            return *this;
        }

        out = read_raw_bytes(count);

        return *this;
    }

    [[nodiscard]] auto bytes_read() const noexcept -> std::size_t
    {
        return m_position;
    }

    [[nodiscard]] auto remaining() const noexcept -> std::size_t
    {
        return m_buffer.size() - m_position;
    }

    [[nodiscard]] auto error() const noexcept -> std::optional<serialization_error_k>
    {
        return m_error;
    }

    [[nodiscard]] auto has_error() const noexcept -> bool
    {
        return m_error.has_value();
    }

    [[nodiscard]] auto eof() const noexcept -> bool
    {
        return m_position == m_buffer.size();
    }

  private:
    auto read_raw_bytes(std::size_t count) noexcept -> std::span<std::byte>
    {
        if (count + m_position > m_buffer.size())
        {
            m_error = serialization_error_k::truncated_file_k;
            return {};
        }

        auto result{m_buffer.subspan(m_position, count)};
        m_position += count;
        return result;
    }

    std::span<std::byte>                 m_buffer;
    std::size_t                          m_position{0};
    std::optional<serialization_error_k> m_error{std::nullopt};
};

} // namespace serialization
