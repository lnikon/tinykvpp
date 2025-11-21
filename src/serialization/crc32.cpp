#include "serialization/crc32.h"
#include <cstddef>
#include <cstdint>

namespace serialization
{

// update updates the stored crc with new data, without XORing with 0xFFFFFFFF
void crc32_t::update(std::span<const std::byte> data) noexcept
{
    for (const std::byte byte : data)
    {
        const std::uint32_t index{(std::to_integer<std::uint32_t>(byte) ^ m_crc) & 0xFF};

        m_crc = (m_crc >> detail::CRC32_BITS) ^ TABLE[index];
    }
}

// finalize returns the stored crc XORed with 0xFFFFFFFF
[[nodiscard]] auto crc32_t::finalize() const noexcept -> std::uint32_t
{
    return m_crc ^ detail::CRC32_DEFAULT_VALUE;
}

// reset sets crc to 0xFFFFFFFF
void crc32_t::reset() noexcept
{
    m_crc = detail::CRC32_DEFAULT_VALUE;
}

} // namespace serialization
