#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace serialization
{

namespace detail
{

static constexpr const std::uint32_t CRC32_DEFAULT_VALUE{0xFFFFFFFF};
static constexpr const std::int32_t  CRC32_BITS{8};
static constexpr const std::size_t   CRC32_TABLE_SIZE{256};
static constexpr const std::uint32_t CRC32_POLYNOMIAL = 0xEDB88320;

using crc32_table_t = std::array<std::uint32_t, CRC32_TABLE_SIZE>;

constexpr auto generate_crc32_table() noexcept -> crc32_table_t
{
    crc32_table_t table{};
    for (std::uint32_t i{0}; i < table.size(); i++)
    {
        std::uint32_t crc = i;
        for (std::uint8_t bit{0}; bit < CRC32_BITS; bit++)
        {
            if ((crc & 1) != 0)
            {
                crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
            }
            else
            {
                crc = crc >> 1;
            }
        }
        table[i] = crc;
    }
    return table;
}

} // namespace detail

class crc32_t final
{
  public:
    constexpr crc32_t() = default;

    // update updates the stored crc with new data, without XORing with 0xFFFFFFFF
    void update(std::span<const std::byte> data) noexcept;

    // finalize returns the stored crc XORed with 0xFFFFFFFF
    [[nodiscard]] auto finalize() const noexcept -> std::uint32_t;

    // reset sets crc to 0xFFFFFFFF
    void reset() noexcept;

  private:
    static constexpr const auto TABLE{detail::generate_crc32_table()};

    std::uint32_t m_crc{detail::CRC32_DEFAULT_VALUE};
};

} // namespace serialization
