#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace frankie::core {

constexpr std::uint32_t kCRC32DefaultValue{0xFFFFFFFF};
constexpr std::uint32_t kCRC32Bits{8};
constexpr std::size_t kCRC32TableSize{256};
constexpr std::uint32_t kCRC32Polynomial = 0xEDB88320;

using crc32_table = std::array<std::uint32_t, kCRC32TableSize>;

constexpr auto generate_crc32_table() noexcept -> crc32_table {
  crc32_table table{};
  for (std::uint32_t i{0}; i < table.size(); i++) {
    std::uint32_t crc = i;
    for (std::uint8_t bit{0}; bit < kCRC32Bits; bit++) {
      if ((crc & 1) != 0) {
        crc = (crc >> 1) ^ kCRC32Polynomial;
      } else {
        crc = crc >> 1;
      }
    }
    table[i] = crc;
  }
  return table;
}

class crc32 final {
 public:
  constexpr crc32() = default;

  // update updates the stored crc with new data, without XORing with 0xFFFFFFFF
  [[nodiscard]] crc32 &update(std::span<const std::byte> data) noexcept;

  // finalize returns the stored crc XORed with 0xFFFFFFFF
  [[nodiscard]] std::uint32_t finalize() const noexcept;

  // reset sets crc to 0xFFFFFFFF
  void reset() noexcept;

 private:
  static constexpr const auto TABLE{generate_crc32_table()};

  std::uint32_t crc_{kCRC32DefaultValue};
};

}  // namespace frankie::core
