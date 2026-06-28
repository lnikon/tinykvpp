#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "core/crc32.hpp"

using namespace frankie::core;

namespace {

// View a string's contents as a span of raw bytes for crc32::update.
[[nodiscard]] auto bytes_of(const std::string &str) noexcept -> std::span<const std::byte> {
  return std::as_bytes(std::span{str});
}

}  // namespace

TEST(Crc32Test, TableGeneration) {
  // Known values from the standard IEEE 802.3 / ZIP / PNG CRC32 table.
  constexpr auto table = generate_crc32_table();
  EXPECT_EQ(table[0], 0x00000000u);
  EXPECT_EQ(table[1], 0x77073096u);
  EXPECT_EQ(table[2], 0xEE0E612Cu);
  EXPECT_EQ(table[255], 0x2D02EF8Du);
}

TEST(Crc32Test, EmptyInput) {
  // CRC32 of no data finalizes to 0.
  crc32 crc;
  EXPECT_EQ(crc.finalize(), 0x00000000u);
}

TEST(Crc32Test, KnownValues) {
  // CRC32("123456789") == 0xCBF43926
  const std::string digits = "123456789";
  crc32 crc1;
  (void)crc1.update(bytes_of(digits));
  EXPECT_EQ(crc1.finalize(), 0xCBF43926u);

  // CRC32("The quick brown fox jumps over the lazy dog") == 0x414FA339
  const std::string fox = "The quick brown fox jumps over the lazy dog";
  crc32 crc2;
  (void)crc2.update(bytes_of(fox));
  EXPECT_EQ(crc2.finalize(), 0x414FA339u);
}

TEST(Crc32Test, IncrementalEqualsSingle) {
  const std::string data = "Hello, World!";

  crc32 single;
  (void)single.update(bytes_of(data));
  const std::uint32_t single_result = single.finalize();

  crc32 chunked;
  (void)chunked.update(bytes_of(data.substr(0, 5)));  // "Hello"
  (void)chunked.update(bytes_of(data.substr(5, 2)));  // ", "
  (void)chunked.update(bytes_of(data.substr(7)));     // "World!"
  const std::uint32_t chunked_result = chunked.finalize();

  EXPECT_EQ(single_result, chunked_result);
}
