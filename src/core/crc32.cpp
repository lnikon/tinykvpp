#include <cstddef>
#include <cstdint>

#include "core/crc32.hpp"

namespace frankie::core {

// update updates the stored crc with new data, without XORing with 0xFFFFFFFF
[[nodiscard]] crc32 &crc32::update(std::span<const std::byte> data) noexcept {
  for (const std::byte byte : data) {
    const std::uint32_t index{(std::to_integer<std::uint32_t>(byte) ^ crc_) & 0xFF};

    crc_ = (crc_ >> kCRC32Bits) ^ TABLE[index];
  }
  return *this;
}

// finalize returns the stored crc XORed with 0xFFFFFFFF
[[nodiscard]] std::uint32_t crc32::finalize() const noexcept { return crc_ ^ kCRC32DefaultValue; }

// reset sets crc to 0xFFFFFFFF
void crc32::reset() noexcept { crc_ = kCRC32DefaultValue; }

}  // namespace frankie::core
