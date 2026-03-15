#pragma once

#include <cstdint>
#include <string_view>

namespace core {

enum class error_code : std::uint8_t {
  put_failed = 0,
};

class error final {
  error(error_code code, std::string_view msg) : code_{code}, msg_{msg} {}

  [[nodiscard]] error_code code() const noexcept { return code_; }

  [[nodiscard]] std::string_view msg() const noexcept { return msg_; }

 private:
  error_code code_;
  std::string_view msg_;
};

}  // namespace core
