#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace serialization
{

// serialization_error_k represents common serialization errors
enum class serialization_error_k : std::int8_t
{
    undefined_k = -1,

    // Write errors
    buffer_overflow_k,

    // Read errors
    truncated_file_k,

    // Format errors
    invalid_magic_k,
    version_mismatch_k,
    checksum_mismatch_k,
    corrupted_data_k,

    // Encoding errors
    invalid_variant_k,
    invalid_offset_k
};

// MAX_VARINT_BYTES represents maximum number of bytes required to encode 64 bit integer with varint
constexpr const std::size_t MAX_VARINT_BYTES{10};

// byte_t represents a single byte
using byte_t = std::byte;

// to_span converts std::string_view into std::byte array
[[nodiscard]] auto to_span(std::string_view view) noexcept -> std::span<const std::byte>;

// to_string_view converts std::byte span into a std::string_view
[[nodiscard]] auto to_string_view(std::span<std::byte> span) noexcept -> std::string_view;

} // namespace serialization
