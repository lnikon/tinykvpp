#pragma once

#include <cstdint>
#include <expected>
#include <type_traits>
#include <utility>

namespace frankie::core {

// Coarse status codes grouped by recovery action.
enum class status_code : std::uint8_t { ok = 0, not_found, io_error, invalid_argument, corrupted, eof, out_of_memory };

[[nodiscard]] constexpr const char *to_cstring(status_code code) noexcept {
  switch (code) {
    case status_code::ok:
      return "ok";
    case status_code::not_found:
      return "not_found";
    case status_code::io_error:
      return "io_error";
    case status_code::invalid_argument:
      return "invalid_argument";
    case status_code::corrupted:
      return "corrupted";
    case status_code::eof:
      return "eof";
    case status_code::out_of_memory:
      return "out_of_memory";
  }
  return "unknown";
}

// Lighweight, trivially copyable status.
//
// Lifetime contract: msg_ must point to storage with static lifetime (typically a string literal).
// Status never copies or owns the pointer, it borrows.
// std::string::c_str() or a std::format() passed as a msg_ will result in a dangling pointer.
//
// OK convention: a default-constructed status is a success.
// Operator bool returns true on success, so call sites can write:
//
// if (auto status = wal_.append(rec); !s) { }
struct [[nodiscard("status can not be discarded")]] status final {
  status_code code_{status_code::ok};
  const char *msg_{""};

  constexpr status() noexcept = default;

  constexpr explicit status(status_code code) : code_{code} {}

  constexpr explicit status(status_code code, const char *msg) : code_{code}, msg_{msg != nullptr ? msg : ""} {}

  [[nodiscard]] constexpr bool ok() const noexcept { return code_ == status_code::ok; }

  [[nodiscard]] constexpr explicit operator bool() const noexcept { return ok(); }
};

// Verifications that status is a simple class.
static_assert(sizeof(status) <= 16, "status should fit into two registers");
static_assert(std::is_trivially_copyable_v<status>, "status must be trivially copyable to pass by value cheaply");

// Helpers to wrap status into std types.
[[nodiscard]] constexpr auto unexpected(status_code code, const char *msg = nullptr) noexcept {
  return std::unexpected<status>(std::in_place, code, msg);
}

[[nodiscard]] constexpr auto unexpected(status st) noexcept { return std::unexpected<status>(std::in_place, st); }

}  // namespace frankie::core
