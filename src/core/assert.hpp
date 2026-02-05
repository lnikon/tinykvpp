#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <utility>

// DELETE ME
#include <print>

// ================================================================================
// Configuration
// ================================================================================

#ifndef FR_ASSERT_LEVEL
#ifdef NDEBUG
#define FR_ASSERT_LEVEL 1
#else
#define FR_ASSERT_LEVEL 3
#endif
#endif

#if __cplusplus >= 202002L && __has_include(<source_location>)
#include <source_location>
#define FR_ASSERT_HAS_SOURCE_LOCATION 1
#else
#define FR_ASSERT_HAS_SOURCE_LOCATION 0
#endif

namespace frankie {
namespace assert_detail {

struct source_location {
  const char* file_;
  const char* function_;
  std::uint32_t line_;

  constexpr source_location(const char* file = __builtin_FILE(),
                            const char* function = __builtin_FUNCTION(),
                            std::uint32_t line = __builtin_LINE()) noexcept
      : file_(file), function_(function), line_(line) {}

#if FR_ASSERT_HAS_SOURCE_LOCATION
  constexpr source_location(const std::source_location& location) noexcept
      : file_(location.file_name()),
        function_(location.function_name()),
        line_(location.line()) {}
};
#endif

// ================================================================================
// Expression decomposition
// ================================================================================

// Forward declaration
template <typename T>
struct expression_value;

template <typename L, typename R, typename Op>
struct binary_expression;

template <typename T>
inline void print_value(char* buf, std::uint64_t buf_size, const T& value) {
  using Type = std::decay_t<T>;
  if constexpr (std::is_same_v<Type, bool>) {
    std::snprintf(buf, buf_size, "%s", value ? "true" : "false");
  } else if constexpr (std::is_same_v<Type, char>) {
    std::snprintf(buf, buf_size, "'%c' (0x%02x)", value,
                  static_cast<unsigned char>(value));
  } else if constexpr (std::is_integral_v<Type>) {
    if (std::is_signed_v<Type>) {
      std::snprintf(buf, buf_size, "%lld", static_cast<long long>(value));
    } else {
      std::snprintf(buf, buf_size, "%llu",
                    static_cast<unsigned long long>(value));
    }
  } else if constexpr (std::is_floating_point_v<Type>) {
    std::snprintf(buf, buf_size, "%g", static_cast<double>(value));
  } else if constexpr (std::is_same_v<Type, const char*> ||
                       std::is_same_v<Type, char*>) {
    if (value == nullptr) {
      std::snprintf(buf, buf_size, "nullptr");
    } else {
      std::snprintf(buf, buf_size, "\"%.*s\"", static_cast<int>(buf_size - 3),
                    value);
    }
  } else if constexpr (std::is_pointer_v<Type>) {
    if (value == nullptr) {
      std::snprintf(buf, buf_size, "nullptr");
    } else {
      std::snprintf(buf, buf_size, "%p", static_cast<const void*>(value));
    }
  } else {
    std::snprintf(buf, buf_size, "<unprintable>");
  }
}

template <typename T>
struct expression_value {
  T value_;

  constexpr explicit expression_value(T&& v) noexcept
      : value_(std::forward<T>(v)) {}

  constexpr operator bool() noexcept {
    if constexpr (std::is_convertible_v<T, bool>) {
      return static_cast<bool>(value_);
    } else {
      return true;
    }
  }

  void print_to(char* buf, std::uint64_t size) const noexcept {
    print_value(buf, size, value_);
  }

#define FR_ASSERT_DEFINE_BINARY_OP(op, op_name)                           \
  template <typename R>                                                   \
  constexpr auto operator op(R&& rhs) const noexcept {                    \
    return binary_expression<expression_value<T>, expression_value<R>,    \
                             struct op_name>{                             \
        *this, expression_value<R>(std::forward<R>(rhs), value_ op rhs)}; \
  }

  struct op_eq {};
  struct op_ne {};
  struct op_lt {};
  struct op_le {};
  struct op_gt {};
  struct op_ge {};

  FR_ASSERT_DEFINE_BINARY_OP(==, op_eq)

#undef FR_ASSERT_DEFINE_BINARY_OP
};

template <typename L, typename R, typename Op>
struct binary_expression {
  L lhs;
  R rhs;
  bool result;

  constexpr operator bool() const noexcept { return result; }

  static constexpr const char* op_str() noexcept {
    if constexpr (std::is_same_v<Op, typename expression_value<int>::op_eq>) {
      return "==";
    } else if constexpr (std::is_same_v<
                             Op, typename expression_value<int>::op_ne>) {
      return "!=";
    } else if constexpr (std::is_same_v<
                             Op, typename expression_value<int>::op_lt>) {
      return "<";
    } else if constexpr (std::is_same_v<
                             Op, typename expression_value<int>::op_le>) {
      return "<=";
    } else if constexpr (std::is_same_v<
                             Op, typename expression_value<int>::op_gt>) {
      return ">";
    } else if constexpr (std::is_same_v<
                             Op, typename expression_value<int>::op_ge>) {
      return ">=";
    } else {
      return "<unrecognized-operation>";
    }
  }

  void format_details(char* buf, std::uint64_t size) const noexcept {
    char lhs_buf[64];
    char rhs_buf[64];
    lhs.print_to(lhs_buf, sizeof(lhs_buf));
    rhs.print_to(rhs_buf, sizeof(rhs_buf));
    std::snprintf(buf, size, "\tLHS: %s\n\tRHS: %s", lhs_buf, rhs_buf);
  }

  // Captures left side of an expression
  struct decomposer {
    template <typename T>
    constexpr auto operator<<(T&& value) const noexcept {
      return expression_value<T>{std::forward<T>(value)};
    }
  };
};

// ================================================================================
// Assertion level
// ================================================================================

enum class assert_level {
  debug,   // Only in debug builds (ASSERT_LEVEL >= 3)
  assume,  // Optimizer hints, checked in debug (ASSERT_LEVEL >= 2)
  verify,  // Always checked, even in release builds (ASSERT_LEVEL >= 1)
  panic    // Always active, unconditional failure
};

// ================================================================================
// Failure handler
// ================================================================================

using failure_handler =
    void (*)(assert_level level, const char* expression, const char* message,
             const assert_detail::source_location& loc) noexcept;

inline void default_failure_handler(
    assert_level level, const char* expression, const char* message,
    const assert_detail::source_location& loc) noexcept {
  const char* level_str = [level]() {
    switch (level) {
      case assert_level::debug: {
        return "DEBUG_ASSERT";
      }
      case assert_level::assume: {
        return "ASSUMPTION";
      }
      case assert_level::verify: {
        return "VERIFICATION";
      }
      case assert_level::panic: {
        return "PANIC";
      }
      default: {
        return "ASSERTION";
      }
    }
  }();

  std::fprintf(stderr,
               "\n"
               "==============================================================="
               "=================\n"
               "%s FAILED\n"
               "---------------------------------------------------------------"
               "-----------------\n"
               "Location: %s:%d\n"
               "Function: %s\n"
               "Expression: %s\n",
               level_str, loc.file_, loc.line_, loc.function_, expression);

  if (message && std::strlen(message) > 0 && message[0] != '\0') {
    std::fprintf(stderr, "Detailed:\n%s\n", message);
  }

  std::fprintf(stderr,
               "---------------------------------------------------------------"
               "-----------------\n\n");
  std::fflush(stderr);
}

// Global failure handler.
// Returns a reference to a global, static object.
// Can be replaced by the caller for custom handler.
inline failure_handler& get_failure_handler() noexcept {
  static failure_handler handler = default_failure_handler;
  return handler;
}

inline void set_failure_handler(failure_handler handler) noexcept {
  get_failure_handler() = handler ? handler : default_failure_handler;
}

// ================================================================================
// Core Assertion Implementation
// ================================================================================

[[noreturn, gnu::cold, gnu::noinline]]
inline void assertion_failed(
    assert_level level, const char* expression, const char* details,
    const assert_detail::source_location& loc) noexcept {
  get_failure_handler()(level, expression, details, loc);

  // Attempt to break into a debuffer if available
#if defined(__has_builtin) && __has_builtin(__builtin_debugtrap)
  __builtin_debugtrap();
#elif defined(__x86_64__) || defined(__i386__)
    __asm__ volatile("int3");
#endif

  std::abort();
}

// SFINAE helper to detect format_details member
template <typename T, typename = void>
struct has_format_details : std::false_type {};

template <typename T>
struct has_format_details<
    T, std::void_t<decltype(std::declval<const T&>().format_details(
           std::declval<char*>(), std::declval<std::uint64_t()>))>>
    : std::true_type {};

template <typename T>
constexpr bool has_format_details_v = has_format_details<T>::value;

template <typename Expr>
[[gnu::always_inline]]
inline void check_assertion(assert_level level, const Expr& expression,
                            const char* expression_str,
                            const assert_detail::source_location& location =
                                assert_detail::source_location{}) {
  if (!static_cast<bool>(expression)) {
    constexpr const std::uint64_t k_details_size{256};
    char details[k_details_size] = {};

    if constexpr (has_format_details_v<Expr>) {
      expression.format_details(details, sizeof(details));
    }

    assertion_failed(level, expression_str, details, location);
  }
}

template <typename Expr>
[[gnu::always_inline]]
inline void check_assertion_msg(assert_level level, const Expr& expression,
                                const char* expression_str, const char* message,
                                const assert_detail::source_location& location =
                                    assert_detail::source_location{}) {
  if (!static_cast<bool>(expression)) {
    constexpr const std::uint64_t k_details_size = 1024;
    char details[k_details_size] = {};
    std::uint64_t offset = 0;

    if constexpr (has_format_details_v<Expr>) {
      expression.format_details(details, sizeof(details));
      offset = std::strlen(details);
      if (offset > 0 && offset < k_details_size - 2) {
        details[offset++] = '\0';
      }
    }

    if (message && offset < k_details_size - 1) {
      std::snprintf(details + offset, k_details_size - offset, "Message: %s",
                    message);
    }

    assertion_failed(level, expression_str, details, location);
  }
}

}  // namespace assert_detail
}  // namespace frankie

// ================================================================================
// Public macros
// ================================================================================

// Helper to stringify the expression
#define FR_ASSERT_STRINGIFY_IMPL(x) #x
#define FR_ASSERT_STRINGIFY(x) FR_ASSERT_STRINGIFY_IMPL(x)

// Expression decomposer wrapper
#define FR_DECOMPOSE_EXPRESSION(expr) (::assert_detail::decomposer{} << expr

// DEBUG_ASSERT - only active when ASSERT_LEVEL >= 3
#if FR_ASSERT_LEVEL >= 3
#define FR_DEBUG_ASSERT(expr)                        \
  ::frankie::assert_detail::check_assertion(         \
      ::frankie::assert_detail::assert_level::debug, \
      FR_DECOMPOSE_EXPRESSION(expr), FR_ASSERT_STRINGIFY(expr))

#define FR_DEBUG_ASSERT_MSG(expr, msg)               \
  ::frankie::assert_detail::check_assertion(         \
      ::frankie::assert_detail::assert_level::debug, \
      FR_DECOMPOSE_EXPRESSION(expr), FR_ASSERT_STRINGIFY(expr), msg)
#else
#define DEBUG_ASSERT(expr) ((void)0)
#define DEBUG_ASSERT_MSG(expr, msg) ((void)0)
#endif

// ASSUME - only active when ASSERT_LEVEL >= 2
#if FR_ASSERT_LEVEL >= 2
#define FR_ASSUME(expr)                               \
  ::frankie::assert_detail::check_assertion(          \
      ::frankie::assert_detail::assert_level::assume, \
      FR_DECOMPOSE_EXPRESSION(expr), FR_ASSERT_STRINGIFY(expr))

#define FR_ASSUME_MSG(expr, msg)                      \
  ::frankie::assert_detail::check_assertion(          \
      ::frankie::assert_detail::assert_level::assume, \
      FR_DECOMPOSE_EXPRESSION(expr), FR_ASSERT_STRINGIFY(expr), msg)
#else
#define ASSUME(expr) ((void)0)
#define ASSUME(expr, msg) ((void)0)
#endif

// ASSUME - only active when ASSERT_LEVEL >= 2
#if FR_ASSERT_LEVEL >= 1
#define FR_VERIFY(expr)                               \
  ::frankie::assert_detail::check_assertion(          \
      ::frankie::assert_detail::assert_level::verify, \
      FR_DECOMPOSE_EXPRESSION(expr), FR_ASSERT_STRINGIFY(expr))

#define FR_VERIFY_MSG(expr, msg)                      \
  ::frankie::assert_detail::check_assertion(          \
      ::frankie::assert_detail::assert_level::verify, \
      FR_DECOMPOSE_EXPRESSION(expr), FR_ASSERT_STRINGIFY(expr), msg)
#else
#define VERIFY(expr) ((void)0)
#define VERIFY(expr, msg) ((void)0)
#endif

// FR_PANIC - always active, unconditional failure
#define FR_PANIC(msg)                                              \
  ::frankie::assert_detail::assertion_failed(                      \
      ::frankie::assert_detail::assert_level::panic, "PANIC", msg, \
      ::frankie::assert_detail::source_location{})

// FR_UNREACHABLE - marks unreachable code
#define FR_UNREACHABLE()                                            \
  ::frankie::assert_detail::assertion_failed(                       \
      ::frankie::assert_detail::assert_level::panic, "UNREACHABLE", \
      "Code marked as unreachable was executed",                    \
      ::frankie::assert_detail::source_location{})

// FR_NOT_IMPLEMENTED - Marks unimplemented code paths
#define FR_NOT_IMPLEMENTED()                                            \
  ::frankie::assert_detail::assertion_failed(                           \
      ::frankie::assert_detail::assert_level::panic, "NOT_IMPLEMENTED", \
      "This functionality is not yet implemented",                      \
      ::frankie::assert_detail::source_location{})

// ================================================================================
// Utility macros
// ================================================================================

#define FR_ASSERT_NOT_NULL(ptr)                                            \
  ([](auto&& p, ::frankie::assert_detail::source_location location = {})   \
       -> decltype(auto) {                                                 \
    if (p == nullptr) [[unlikely]] {                                       \
      ::frankie::assert_detail::assertion_failed(                          \
          ::frankie::assert_detail::assert_level::panic,                   \
          FR_ASSERT_STRINGIFY(p) " != nullptr", "Unexpected null pointer", \
          location);                                                       \
    }                                                                      \
    return std::forward<decltype(p)>(p);                                   \
  })(ptr);

#define FR_ASSERT_VAL(expression, expected)                                 \
  ([](auto&& val,                                                           \
      ::frankie::assert_detail::source_location location = {}) -> void {    \
    if (!(val == (expected))) [[unlikely]] {                                \
      constexpr const std::uint64_t k_buffer_size = 1024;                   \
      char details[k_buffer_size];                                          \
      ::frankie::assert_detail::print_value(details, sizeof(details), val); \
      char msg[k_buffer_size];                                              \
      std::snprintf(msg, sizeof(msg),                                       \
                    "Expected: " FR_ASSERT_STRINGIFY(expected) ", Got: %s", \
                    details);                                               \
      ::frankie::assert_detail::assertion_failed(                           \
          ::frankie::assert_detail::assert_level::verify,                   \
          FR_ASSERT_STRINGIFY(expression) " != " FR_ASSERT_STRINGIFY(       \
              expected),                                                    \
          msg, location);                                                   \
    }                                                                       \
  })(expression);
