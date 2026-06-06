#include <span>
#include <string_view>

#include "core/serialization/common.hpp"

namespace frankie::core {

std::span<const std::byte> to_span(std::string_view view) noexcept {
  return std::as_bytes(std::span(view.data(), view.size()));
}

std::span<std::byte> to_writable_span(char *data, std::size_t size) noexcept {
  return std::span{reinterpret_cast<std::byte *>(data), size};
}

std::string_view to_string_view(std::span<std::byte> span) noexcept {
  if (span.empty()) {
    return {};
  }

  return {reinterpret_cast<const char *>(span.data()), span.size()};
}

std::size_t varint_size(std::size_t value) noexcept {
  std::size_t count{1};
  while (value >= 128) {
    value >>= 7;
    count++;
  }
  return count;
}

}  // namespace frankie::core
