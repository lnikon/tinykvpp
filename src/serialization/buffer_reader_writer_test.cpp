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
    const std::int8_t podInt8{12};
    const le_int8_t   expectedInt8{podInt8};

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
    std::string          actualString;
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
