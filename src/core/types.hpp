#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

namespace types {

template <typename Tag, typename T>
class strong_type_t final {
  T m_value;

 public:
  using underlying_type = T;
  using tag_type = Tag;

  constexpr explicit strong_type_t(T value) noexcept(
      std::is_nothrow_move_constructible_v<T>)
      : m_value(std::move(value)) {}

  constexpr T& get() & noexcept { return m_value; }
  constexpr const T& get() const& noexcept { return m_value; }
  constexpr const T&& get() const&& noexcept { return m_value; }
};

// =============================================================================
// Tags
// =============================================================================
struct comparable_tag_t final {};
struct ordered_tag_t final {};
struct arithmetic_tag_t final {};

// =============================================================================
// Comparable
// =============================================================================
template <typename Tag, typename T>
  requires(std::derived_from<Tag, comparable_tag_t> &&
           std::equality_comparable<T>)
constexpr bool operator==(const strong_type_t<Tag, T>& lhs,
                          const strong_type_t<Tag, T>& rhs) {
  return lhs == rhs;
}

// =============================================================================
// Ordering
// =============================================================================
template <typename Tag, typename T>
  requires(std::derived_from<Tag, ordered_tag_t> &&
           std::three_way_comparable<T>)
constexpr bool operator<=>(const strong_type_t<Tag, T>& lhs,
                           const strong_type_t<Tag, T>& rhs) {
  return lhs <=> rhs;
}

// =============================================================================
// Arithmetic
// =============================================================================
template <typename Tag, typename T>
  requires(std::derived_from<Tag, ordered_tag_t> &&
           std::three_way_comparable<T>)
constexpr bool operator+(const strong_type_t<Tag, T>& lhs,
                         const strong_type_t<Tag, T>& rhs) {
  return strong_type_t<Tag, T>{lhs.get() + rhs.get()};
}

template <typename Tag, typename T>
  requires(std::derived_from<Tag, ordered_tag_t> &&
           std::three_way_comparable<T>)
constexpr bool operator-(const strong_type_t<Tag, T>& lhs,
                         const strong_type_t<Tag, T>& rhs) {
  return strong_type_t<Tag, T>{lhs.get() - rhs.get()};
}

template <typename Tag, typename T>
  requires(std::derived_from<Tag, ordered_tag_t> &&
           std::three_way_comparable<T>)
constexpr strong_type_t<Tag, T>& operator++(strong_type_t<Tag, T>& a) {
  ++a.get();
  return a;
}

template <typename Tag, typename T>
  requires(std::derived_from<Tag, ordered_tag_t> &&
           std::three_way_comparable<T>)
constexpr strong_type_t<Tag, T> operator++(strong_type_t<Tag, T>& a) {
  auto copy{a};
  ++a.get();
  return copy;
}
}  // namespace types
