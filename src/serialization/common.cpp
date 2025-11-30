#include "serialization/common.h"

namespace serialization
{

[[nodiscard]] auto to_span(std::string_view view) noexcept -> std::span<const std::byte>
{
    return std::as_bytes(std::span(view.data(), view.size()));
}

[[nodiscard]] auto to_string_view(std::span<std::byte> span) noexcept -> std::string_view
{
    if (span.empty())
    {
        return {};
    }

    return {reinterpret_cast<const char *>(span.data()), span.size()};
}

[[nodiscard]] auto varint_size(std::size_t value) noexcept -> std::size_t
{
    std::size_t count{1};
    while (value >= 128)
    {
        value >>= 7;
        count++;
    }
    return count;
}

} // namespace serialization
