#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "core/serialization/codec.hpp"
#include "core/views.hpp"

namespace frankie::core {
namespace {

// Round-trips a value through store_le -> load_le and asserts equality.
template <std::integral T>
void expect_le_round_trip(T value) {
  std::array<std::byte, sizeof(T)> buf{};
  codec::store_le<T>(value, std::span<std::byte, sizeof(T)>{buf});
  EXPECT_EQ((codec::load_le<T>(std::span<const std::byte, sizeof(T)>{buf})), value);
}

// Encodes a varint into a max-sized scratch buffer and returns the byte count.
std::size_t varint_size(std::uint64_t value) {
  std::array<std::byte, kMaxVarintBytes> buf{};
  return codec::encode_varint(value, buf);
}

TEST(CodecLoadStoreTest, RoundTripInt8) {
  expect_le_round_trip<std::int8_t>(std::numeric_limits<std::int8_t>::min());
  expect_le_round_trip<std::int8_t>(-1);
  expect_le_round_trip<std::int8_t>(0);
  expect_le_round_trip<std::int8_t>(1);
  expect_le_round_trip<std::int8_t>(std::numeric_limits<std::int8_t>::max());
}

TEST(CodecLoadStoreTest, RoundTripUint8) {
  expect_le_round_trip<std::uint8_t>(0);
  expect_le_round_trip<std::uint8_t>(1);
  expect_le_round_trip<std::uint8_t>(std::numeric_limits<std::uint8_t>::max());
}

TEST(CodecLoadStoreTest, RoundTripInt16) {
  expect_le_round_trip<std::int16_t>(std::numeric_limits<std::int16_t>::min());
  expect_le_round_trip<std::int16_t>(-1);
  expect_le_round_trip<std::int16_t>(0);
  expect_le_round_trip<std::int16_t>(12345);
  expect_le_round_trip<std::int16_t>(std::numeric_limits<std::int16_t>::max());
}

TEST(CodecLoadStoreTest, RoundTripUint16) {
  expect_le_round_trip<std::uint16_t>(0);
  expect_le_round_trip<std::uint16_t>(static_cast<std::uint16_t>(0x0102));
  expect_le_round_trip<std::uint16_t>(std::numeric_limits<std::uint16_t>::max());
}

TEST(CodecLoadStoreTest, RoundTripInt32) {
  expect_le_round_trip<std::int32_t>(std::numeric_limits<std::int32_t>::min());
  expect_le_round_trip<std::int32_t>(-1);
  expect_le_round_trip<std::int32_t>(0);
  expect_le_round_trip<std::int32_t>(0x01020304);
  expect_le_round_trip<std::int32_t>(std::numeric_limits<std::int32_t>::max());
}

TEST(CodecLoadStoreTest, RoundTripUint32) {
  expect_le_round_trip<std::uint32_t>(0);
  expect_le_round_trip<std::uint32_t>(0xDEADBEEFU);
  expect_le_round_trip<std::uint32_t>(std::numeric_limits<std::uint32_t>::max());
}

TEST(CodecLoadStoreTest, RoundTripInt64) {
  expect_le_round_trip<std::int64_t>(std::numeric_limits<std::int64_t>::min());
  expect_le_round_trip<std::int64_t>(-1);
  expect_le_round_trip<std::int64_t>(0);
  expect_le_round_trip<std::int64_t>(0x0102030405060708LL);
  expect_le_round_trip<std::int64_t>(std::numeric_limits<std::int64_t>::max());
}

TEST(CodecLoadStoreTest, RoundTripUint64) {
  expect_le_round_trip<std::uint64_t>(0);
  expect_le_round_trip<std::uint64_t>(0x0102030405060708ULL);
  expect_le_round_trip<std::uint64_t>(std::numeric_limits<std::uint64_t>::max());
}

TEST(CodecStoreLeTest, LittleEndianByteOrderUint16) {
  std::array<std::byte, sizeof(std::uint16_t)> buf{};
  codec::store_le<std::uint16_t>(static_cast<std::uint16_t>(0x0102),
                                 std::span<std::byte, sizeof(std::uint16_t)>{buf});
  EXPECT_EQ(buf[0], std::byte{0x02});
  EXPECT_EQ(buf[1], std::byte{0x01});
}

TEST(CodecStoreLeTest, LittleEndianByteOrderUint32) {
  std::array<std::byte, sizeof(std::uint32_t)> buf{};
  codec::store_le<std::uint32_t>(0x01020304U, std::span<std::byte, sizeof(std::uint32_t)>{buf});
  EXPECT_EQ(buf[0], std::byte{0x04});
  EXPECT_EQ(buf[1], std::byte{0x03});
  EXPECT_EQ(buf[2], std::byte{0x02});
  EXPECT_EQ(buf[3], std::byte{0x01});
}

TEST(CodecVarintTest, EncodeSizes) {
  EXPECT_EQ(varint_size(0U), 1u);
  EXPECT_EQ(varint_size(1U), 1u);
  EXPECT_EQ(varint_size(127U), 1u);
  EXPECT_EQ(varint_size(128U), 2u);
  EXPECT_EQ(varint_size(300U), 2u);
  EXPECT_EQ(varint_size(16384U), 3u);
  // NOTE: spec listed (1u<<20) as 4 bytes, but 2^20 needs 21 bits -> 3 bytes.
  // 2^21 needs 22 bits -> 4 bytes, which is the intended boundary.
  EXPECT_EQ(varint_size(1U << 20), 3u);
  EXPECT_EQ(varint_size(1U << 21), 4u);
  EXPECT_EQ(varint_size(1ULL << 32), 5u);
}

TEST(CodecVarintTest, Encode300ExactBytes) {
  std::array<std::byte, kMaxVarintBytes> buf{};
  const std::size_t count = codec::encode_varint(300U, buf);
  ASSERT_EQ(count, 2u);
  EXPECT_EQ(buf[0], std::byte{0xAC});
  EXPECT_EQ(buf[1], std::byte{0x02});
}

TEST(CodecVarintTest, RoundTrip) {
  constexpr std::array<std::uint64_t, 8> values{
      0U, 1U, 127U, 128U, 300U, 16384U, 1U << 21, 1ULL << 32};
  for (const std::uint64_t value : values) {
    std::array<std::byte, kMaxVarintBytes> buf{};
    const std::size_t count = codec::encode_varint(value, buf);

    std::size_t pos{0};
    const auto decoded = codec::decode_varint(std::span<const std::byte>{buf.data(), count}, pos);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, value);
    EXPECT_EQ(pos, count);
  }
}

TEST(CodecVarintTest, DecodeTruncated) {
  const std::array<std::byte, 1> buf{std::byte{0x80}};
  std::size_t pos{0};
  const auto decoded = codec::decode_varint(buf, pos);
  EXPECT_FALSE(decoded.has_value());
}

TEST(CodecVarintTest, DecodeOverlong) {
  std::array<std::byte, kMaxVarintBytes + 1> buf{};
  buf.fill(std::byte{0x80});
  std::size_t pos{0};
  const auto decoded = codec::decode_varint(buf, pos);
  EXPECT_FALSE(decoded.has_value());
}

}  // namespace
}  // namespace frankie::core
