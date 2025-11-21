#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>

#include "serialization/concepts.h"

namespace serialization
{

template <typename T, std::endian Target = std::endian::little> class endian_integer
{
  public:
    static constexpr size_t SIZE = sizeof(T);

    endian_integer(T value)
    {
        T converted = value;

        if constexpr (std::endian::native != Target)
        {
            std::cout << "converted" << '\n';
            converted = std::byteswap(value);
        }

        std::memcpy(m_bytes.data(), &converted, sizeof(T));
    }

    [[nodiscard]] auto bytes() const noexcept -> std::span<const std::byte>
    {
        return m_bytes;
    }

    [[nodiscard]] auto get() const noexcept -> T
    {
        T value;
        std::memcpy(&value, m_bytes.data(), sizeof(T));

        if constexpr (std::endian::native != Target)
        {
            return std::byteswap(value);
        }

        return value;
    }

    static auto from_bytes(std::span<const std::byte> bytes) -> endian_integer
    {
        endian_integer result{0};
        std::memcpy(result.m_bytes.data(), bytes.data(), bytes.size());
        return result;
    }

  private:
    std::array<std::byte, sizeof(T)> m_bytes;
};

using le_int8_t = endian_integer<std::int8_t, std::endian::little>;
using le_int16_t = endian_integer<std::int16_t, std::endian::little>;
using le_int32_t = endian_integer<std::int32_t, std::endian::little>;
using le_int64_t = endian_integer<std::int64_t, std::endian::little>;
using le_uint8_t = endian_integer<std::uint8_t, std::endian::little>;
using le_uint16_t = endian_integer<std::uint16_t, std::endian::little>;
using le_uint32_t = endian_integer<std::uint32_t, std::endian::little>;
using le_uint64_t = endian_integer<std::uint64_t, std::endian::little>;

} // namespace serialization
