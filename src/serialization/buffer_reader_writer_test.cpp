#include <array>

#include <cstddef>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include "serialization/buffer_reader.h"
#include "serialization/buffer_writer.h"
#include "serialization/endian_integer.h"

using namespace serialization;

class MyFixture : public testing::Test
{
  protected:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

TEST(EndianInteger, Get)
{
    const std::int8_t podInt8{12};
    const le_int8_t   expectedInt8{podInt8};

    EXPECT_EQ(expectedInt8.get(), podInt8);
}

TEST(EndianInteger, FromBytes)
{
    const std::int8_t          podInt8{12};
    const le_int8_t            expectedInt8{podInt8};
    const std::span<std::byte> bytes{};

    EXPECT_EQ(expectedInt8.get(), podInt8);
}

TEST(BufferReaderWriterTest, ReadYourWrites)
{
    // Raw data
    static constexpr const std::uint64_t                     bytesCount{4};
    static constexpr const std::array<std::byte, bytesCount> rawBytes{
        std::byte{0x40}, std::byte{0xFC}, std::byte{0x04}, std::byte{0xAF}
    };

    // Expected values
    const le_int8_t                                   expectedInt8{12};
    const le_int8_t                                   expectedInt8neg{-4};
    static constexpr const std::string                expectedString{"t!s$ a str "};
    static constexpr const std::span<const std::byte> expectedBytes{rawBytes};

    // Buffer to serialize into and deserialize from
    const std::size_t bufferSize{
        sizeof(expectedInt8) + sizeof(expectedInt8neg) + 1 + expectedString.size() +
        sizeof(bytesCount) + expectedBytes.size()
    };
    std::array<std::byte, bufferSize> buffer{};

    // Serilize into buffer
    buffer_writer_t writer(buffer);
    auto            status{writer.write_endian_integer(expectedInt8)
                    .write_endian_integer(expectedInt8neg)
                    .write_string(expectedString)
                    .write_endian_integer(le_uint64_t{bytesCount})
                    .write_bytes(expectedBytes)
                    .has_error()};
    EXPECT_FALSE(status);
    EXPECT_EQ(writer.bytes_written(), bufferSize);

    // Derserialize from buffer
    buffer_reader_t      reader(buffer);
    le_int8_t            int8{0};
    le_int8_t            int8neg{0};
    std::string_view     actualString;
    le_uint64_t          actualBytesCount{0};
    std::span<std::byte> actualBytes;
    status = reader.read_endian_integer(int8)
                 .read_endian_integer(int8neg)
                 .read_string(actualString)
                 .read_endian_integer(actualBytesCount)
                 .read_bytes(bytesCount, actualBytes)
                 .has_error();
    EXPECT_FALSE(status);
    EXPECT_EQ(reader.bytes_read(), writer.bytes_written());

    // Check that we read what we wrote
    EXPECT_EQ(int8.get(), expectedInt8.get());
    EXPECT_EQ(int8neg.get(), expectedInt8neg.get());
    EXPECT_EQ(expectedString, actualString);
    EXPECT_EQ(bytesCount, actualBytesCount.get());
    EXPECT_TRUE(std::ranges::equal(expectedBytes, actualBytes));
}

TEST(BufferReaderWriterTest, VarintSmallValues)
{
    // Values 0-127 should encode as 1 byte
    std::array<std::byte, 10> buffer{};
    buffer_writer_t           writer(buffer);

    writer.write_varint(0);   // Should write: [0x00]
    writer.write_varint(1);   // Should write: [0x01]
    writer.write_varint(127); // Should write: [0x7F]

    EXPECT_FALSE(writer.has_error());
    EXPECT_EQ(writer.bytes_written(), 3); // 1 + 1 + 1 = 3 bytes

    // Read back
    buffer_reader_t reader(buffer);
    uint64_t        v0, v1, v127;
    reader.read_varint(v0).read_varint(v1).read_varint(v127);

    EXPECT_FALSE(reader.has_error());
    EXPECT_EQ(v0, 0);
    EXPECT_EQ(v1, 1);
    EXPECT_EQ(v127, 127);
}

TEST(BufferReaderWriterTest, VarintMediumValues)
{
    // Value 300 from the example (should be 2 bytes: [0xAC, 0x02])
    std::array<std::byte, 10> buffer{};
    buffer_writer_t           writer(buffer);

    writer.write_varint(300);

    EXPECT_FALSE(writer.has_error());
    EXPECT_EQ(writer.bytes_written(), 2);

    // Verify exact encoding
    EXPECT_EQ(buffer[0], std::byte{0xAC});
    EXPECT_EQ(buffer[1], std::byte{0x02});

    // Read back
    buffer_reader_t reader(buffer);
    uint64_t        value;
    reader.read_varint(value);

    EXPECT_FALSE(reader.has_error());
    EXPECT_EQ(value, 300);
}

TEST(BufferReaderWriterTest, VarintLargeValues)
{
    // Test larger values
    std::array<std::byte, 20> buffer{};
    buffer_writer_t           writer(buffer);

    writer.write_varint(16384);      // 3 bytes
    writer.write_varint(1u << 20);   // 4 bytes
    writer.write_varint(1ull << 32); // 5 bytes

    EXPECT_FALSE(writer.has_error());

    buffer_reader_t reader(buffer);
    uint64_t        v1, v2, v3;
    reader.read_varint(v1).read_varint(v2).read_varint(v3);

    EXPECT_FALSE(reader.has_error());
    EXPECT_EQ(v1, 16384);
    EXPECT_EQ(v2, 1u << 20);
    EXPECT_EQ(v3, 1ull << 32);
}

TEST(BufferReaderWriterTest, EmptyString)
{
    // Empty strings should work
    std::array<std::byte, 10> buffer{};
    buffer_writer_t           writer(buffer);

    writer.write_string("");

    EXPECT_FALSE(writer.has_error());
    EXPECT_EQ(writer.bytes_written(), 1); // Just varint(0)

    buffer_reader_t  reader(buffer);
    std::string_view str;
    reader.read_string(str);

    EXPECT_FALSE(reader.has_error());
    EXPECT_TRUE(str.empty());
}

TEST(BufferReaderWriterTest, VarintOverflow)
{
    // Test error handling: reading a malformed varint
    std::array<std::byte, 11> buffer{};

    // Fill with 11 bytes all having continuation bit set
    for (int i = 0; i < 11; i++)
    {
        buffer[i] = std::byte{0xFF};
    }

    buffer_reader_t reader(buffer);
    uint64_t        value;
    reader.read_varint(value);

    // Should error after 10 bytes
    EXPECT_TRUE(reader.has_error());
    EXPECT_EQ(*reader.error(), serialization_error_k::invalid_variant_k);
}
