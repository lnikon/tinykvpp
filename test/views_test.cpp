#include "core/views.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

using namespace frankie::core;

// to_span(view) -> to_string_view(span) round-trips the bytes unchanged, and
// the span length matches the source string length.
TEST(ViewsTest, ToSpanRoundTripsThroughStringView) {
  constexpr std::string_view kInput{"hello world"};
  const std::span<const std::byte> span = to_span(kInput);
  EXPECT_EQ(span.size(), kInput.size());
  EXPECT_EQ(to_string_view(span), kInput);
}

// Digits and punctuation survive the round-trip; length is preserved.
TEST(ViewsTest, ToSpanPreservesAssortedCharacters) {
  constexpr std::string_view kInput{"Hello, World! 0123456789 @#$%^&*()_+-=[]{}"};
  const std::span<const std::byte> span = to_span(kInput);
  EXPECT_EQ(span.size(), kInput.size());
  EXPECT_EQ(to_string_view(span), kInput);
}

// A writable span aliases the backing buffer: writes through the span are
// observable in the buffer, and read back via to_string_view.
TEST(ViewsTest, WritableSpanMutatesBackingBuffer) {
  std::array<char, 3> buf{};  // zero-initialized
  const std::span<std::byte> span = to_writable_span(buf.data(), buf.size());
  ASSERT_EQ(span.size(), buf.size());

  span[0] = std::byte{0x41};  // 'A'
  span[1] = std::byte{0x42};  // 'B'
  span[2] = std::byte{0x43};  // 'C'

  // The backing buffer observed the writes.
  EXPECT_EQ(buf[0], 'A');
  EXPECT_EQ(buf[1], 'B');
  EXPECT_EQ(buf[2], 'C');

  // And the span reads back as the new content.
  EXPECT_EQ(to_string_view(span), "ABC");
}

// Empty input yields an empty span, and an empty span yields an empty view.
TEST(ViewsTest, EmptyStringYieldsEmptySpanAndView) {
  const std::span<const std::byte> span = to_span("");
  EXPECT_TRUE(span.empty());
  EXPECT_EQ(span.size(), 0u);
  EXPECT_TRUE(to_string_view(span).empty());

  // A default-constructed (empty) span also maps to an empty view.
  EXPECT_TRUE(to_string_view(std::span<const std::byte>{}).empty());
}

TEST(ViewsTest, MaxVarintBytesIsTen) {
  EXPECT_EQ(frankie::core::kMaxVarintBytes, 10u);
}
