#include <array>

#include <cstddef>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include "serialization/buffer_reader.h"
#include "serialization/buffer_writer.h"
#include "serialization/common.h"

using namespace serialization;

TEST(BufferReaderWriterTest, VarintSmallValues)
{
    // Values 0-127 should encode as 1 byte
    std::array<std::byte, MAX_VARINT_BYTES> buffer{};
    buffer_writer_t                         writer(buffer);

    (void)writer.write_varint(0);   // Should write: [0x00]
    (void)writer.write_varint(1);   // Should write: [0x01]
    (void)writer.write_varint(127); // Should write: [0x7F]

    EXPECT_FALSE(writer.has_error());
    EXPECT_EQ(writer.bytes_written(), 3); // 1 + 1 + 1 = 3 bytes

    // Read back
    buffer_reader_t reader(buffer);
    uint64_t        v0 = 0;
    uint64_t        v1 = 0;
    uint64_t        v127 = 0;
    (void)reader.read_varint(v0).read_varint(v1).read_varint(v127);

    EXPECT_FALSE(reader.has_error());
    EXPECT_EQ(v0, 0);
    EXPECT_EQ(v1, 1);
    EXPECT_EQ(v127, 127);
}

TEST(BufferReaderWriterTest, VarintMediumValues)
{
    // Value 300 from the example (should be 2 bytes: [0xAC, 0x02])
    std::array<std::byte, MAX_VARINT_BYTES> buffer{};
    buffer_writer_t                         writer(buffer);

    (void)writer.write_varint(300);

    EXPECT_FALSE(writer.has_error());
    EXPECT_EQ(writer.bytes_written(), 2);

    // Verify exact encoding
    EXPECT_EQ(buffer[0], std::byte{0xAC});
    EXPECT_EQ(buffer[1], std::byte{0x02});

    // Read back
    buffer_reader_t reader(buffer);
    uint64_t        value = 0;
    (void)reader.read_varint(value);

    EXPECT_FALSE(reader.has_error());
    EXPECT_EQ(value, 300);
}

TEST(BufferReaderWriterTest, VarintLargeValues)
{
    // Test larger values
    std::array<std::byte, MAX_VARINT_BYTES * 2> buffer{};
    buffer_writer_t                             writer(buffer);

    (void)writer.write_varint(16384);      // 3 bytes
    (void)writer.write_varint(1u << 20);   // 4 bytes
    (void)writer.write_varint(1ull << 32); // 5 bytes

    EXPECT_FALSE(writer.has_error());

    buffer_reader_t reader(buffer);
    uint64_t        v1{0};
    uint64_t        v2{0};
    uint64_t        v3{0};
    (void)reader.read_varint(v1).read_varint(v2).read_varint(v3);

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

    (void)writer.write_string("");

    EXPECT_FALSE(writer.has_error());
    EXPECT_EQ(writer.bytes_written(), 1); // Just varint(0)

    buffer_reader_t reader(buffer);
    std::string     str;
    (void)reader.read_string(str);

    EXPECT_FALSE(reader.has_error());
    EXPECT_TRUE(str.empty());
}

TEST(BufferReaderWriterTest, VarintOverflow)
{
    // Test error handling: reading a malformed varint
    std::array<std::byte, MAX_VARINT_BYTES + 1> buffer{};

    // Fill with 11 bytes all having continuation bit set
    for (int i = 0; i < 11; i++)
    {
        buffer[i] = std::byte{0xFF};
    }

    buffer_reader_t reader(buffer);
    uint64_t        value;
    (void)reader.read_varint(value);

    // Should error after 10 bytes
    EXPECT_TRUE(reader.has_error());
    EXPECT_EQ(*reader.error(), serialization_error_k::invalid_variant_k);
}
