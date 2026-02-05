#include "core/assert.hpp"

#include <gtest/gtest.h>

TEST(AssertTest, PrintValueTest) {
  struct unprintable {};

  constexpr const std::uint64_t k_buf_size{64};
  std::array<char, k_buf_size> buf;

  std::memset(buf.data(), '\0', k_buf_size);
  frankie::assert_detail::print_value(buf.data(), k_buf_size, true);
  EXPECT_STRCASEEQ(buf.data(), "true");

  // True/false
  std::memset(buf.data(), '\0', k_buf_size);
  frankie::assert_detail::print_value(buf.data(), k_buf_size, false);
  EXPECT_STRCASEEQ(buf.data(), "false");

  // Char
  std::memset(buf.data(), '\0', k_buf_size);
  frankie::assert_detail::print_value(buf.data(), k_buf_size, 'l');
  EXPECT_STRCASEEQ(buf.data(), "\'l\' (0x6c)");

  // Signed integer
  std::memset(buf.data(), '\0', k_buf_size);
  frankie::assert_detail::print_value(buf.data(), k_buf_size, -4);
  EXPECT_STRCASEEQ(buf.data(), "-4");

  // Unsigned integer
  std::memset(buf.data(), '\0', k_buf_size);
  frankie::assert_detail::print_value(buf.data(), k_buf_size, 5);
  EXPECT_STRCASEEQ(buf.data(), "5");

  // Float/double
  std::memset(buf.data(), '\0', k_buf_size);
  frankie::assert_detail::print_value(buf.data(), k_buf_size, 0.549);
  EXPECT_STRCASEEQ(buf.data(), "0.549");

  // Char*/const char*
  std::memset(buf.data(), '\0', k_buf_size);
  const char* msg = "a character array\0";
  frankie::assert_detail::print_value(buf.data(), k_buf_size, msg);
  EXPECT_EQ(std::string(buf.data(), std::strlen(msg) + 2),
            std::format("\"{}\"", msg));

  // Nullptr/pointer
  std::memset(buf.data(), '\0', k_buf_size);
  void* pointer = nullptr;
  frankie::assert_detail::print_value(buf.data(), k_buf_size, pointer);
  EXPECT_STRCASEEQ(buf.data(), "nullptr");

  // Unprintable
  frankie::assert_detail::print_value(buf.data(), k_buf_size, unprintable{});
  EXPECT_STRCASEEQ(buf.data(), "<unprintable>");
}
