#pragma once

#include <span>
#include <string_view>

namespace frankie::core {

// MAX_VARINT_BYTES represents maximum number of bytes required to encode 64 bit integer with varint
constexpr const std::size_t kMaxVarintBytes{10};

// to_span converts std::string_view into const std::byte array
[[nodiscard]] auto to_span(std::string_view view) noexcept -> std::span<const std::byte>;

// to_span_mut converts std::string_view into std::byte array
std::span<std::byte> to_writable_span(char *data, std::size_t size) noexcept;
std::span<std::byte> to_writable_span(std::span<char> span) noexcept;

// to_string_view converts std::byte span into a std::string_view
[[nodiscard]] auto to_string_view(std::span<const std::byte> span) noexcept -> std::string_view;

}  // namespace frankie::core
