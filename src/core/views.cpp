#include <span>
#include <string_view>

#include "core/views.hpp"

namespace frankie::core {

std::span<const std::byte> to_span(std::string_view view) noexcept {
  return std::as_bytes(std::span(view.data(), view.size()));
}

std::span<std::byte> to_writable_span(char *data, std::size_t size) noexcept {
  return std::span{reinterpret_cast<std::byte *>(data), size};
}

std::string_view to_string_view(std::span<const std::byte> span) noexcept {
  if (span.empty()) {
    return {};
  }

  return {reinterpret_cast<const char *>(span.data()), span.size()};
}

}  // namespace frankie::core
