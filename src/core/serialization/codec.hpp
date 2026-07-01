#pragma once

#include <bit>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <optional>

#include "core/views.hpp"

namespace frankie::core::codec {

template <std::integral T>
constexpr T load_le(std::span<const std::byte, sizeof(T)> input) {
  T value;
  std::memcpy(&value, input.data(), sizeof(T));
  if constexpr (std::endian::native != std::endian::little) {
    value = std::byteswap(value);
  }
  return value;
}

template <std::integral T>
constexpr void store_le(T value, std::span<std::byte, sizeof(T)> out) noexcept {
  if constexpr (std::endian::native != std::endian::little) {
    value = std::byteswap(value);
  }
  std::memcpy(out.data(), &value, sizeof(T));
}

inline std::size_t encode_varint(std::uint64_t value, std::span<std::byte> out) noexcept {
  std::size_t count{0};
  while (value >= 0x80) {
    out[count++] = std::byte(static_cast<std::uint8_t>(value) | 0x80);
    value >>= 7;
  }
  out[count++] = std::byte{static_cast<std::uint8_t>((value))};
  return count;
}

[[nodiscard]] inline std::optional<std::uint64_t> decode_varint(std::span<const std::byte> input,
                                                                std::size_t &pos) noexcept {
  std::uint64_t result{0};
  std::size_t shift{0};
  for (std::size_t i = 0; i < kMaxVarintBytes; i++) {
    if (pos >= input.size()) {
      return std::nullopt;
    }
    const auto byte = input[pos++];
    result |= static_cast<std::uint64_t>(std::to_integer<std::uint64_t>(byte) & 0x7F) << shift;
    if ((byte & std::byte{0x80}) == std::byte{0}) {
      return result;
    }
    shift += 7;
  }
  return std::nullopt;
}

}  // namespace frankie::core::codec