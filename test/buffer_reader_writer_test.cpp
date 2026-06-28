#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <gtest/gtest.h>

#include "core/serialization/buffer_reader.hpp"
#include "core/serialization/buffer_writer.hpp"
#include "core/status.hpp"
#include "core/views.hpp"

using namespace frankie;

// Write a mix of values, then read them back in the same order and assert they
// round-trip unchanged.
TEST(BufferReaderWriterTest, ReadYourWrites)
{
    constexpr std::uint32_t    expectedU32{0xDEADBEEFU};
    constexpr std::uint64_t    expectedU64{0x0123456789ABCDEFULL};
    constexpr std::uint64_t    expectedVarint{300ULL};
    constexpr std::string_view expectedString{"hello frankie"};
    constexpr std::string_view blobSource{"raw-blob-payload"};

    const std::span<const std::byte> expectedBlob{core::to_span(blobSource)};

    std::array<std::byte, 256> buffer{};

    auto writer{core::buffer_writer::create(buffer)};
    (void)writer.write<std::uint32_t>(expectedU32)
        .write<std::uint64_t>(expectedU64)
        .write_varint(expectedVarint)
        .write_string(expectedString)
        .write_bytes(expectedBlob);

    EXPECT_FALSE(writer.error().has_value());

    core::buffer_reader reader{buffer};

    std::uint32_t              actualU32{0};
    std::uint64_t              actualU64{0};
    std::uint64_t              actualVarint{0};
    std::string_view           actualString;
    std::span<const std::byte> actualBlob;

    (void)reader.read<std::uint32_t>(actualU32)
        .read<std::uint64_t>(actualU64)
        .read_varint(actualVarint)
        .read_string_view(actualString)
        .read_bytes(expectedBlob.size(), actualBlob);

    EXPECT_FALSE(reader.error().has_value());

    EXPECT_EQ(actualU32, expectedU32);
    EXPECT_EQ(actualU64, expectedU64);
    EXPECT_EQ(actualVarint, expectedVarint);
    EXPECT_EQ(actualString, expectedString);
    EXPECT_EQ(core::to_string_view(actualBlob), blobSource);

    EXPECT_EQ(writer.bytes_written(), reader.bytes_read());
}

// Writing more bytes than the target buffer can hold must surface
// buffer_overflow on the writer.
TEST(BufferReaderWriterTest, WriterOverflow)
{
    std::array<std::byte, 2> tiny{};

    auto writer{core::buffer_writer::create(tiny)};
    (void)writer.write<std::uint64_t>(0x1122334455667788ULL);

    EXPECT_TRUE(writer.error().has_value());
    EXPECT_EQ(*writer.error(), core::status_code::buffer_overflow);
}

// A varint made entirely of continuation bytes (high bit set) with no
// terminating byte is corrupted: the reader runs out of input first.
TEST(BufferReaderWriterTest, ReadTruncatedVarint)
{
    std::array<std::byte, 3> truncated{std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}};

    core::buffer_reader reader{truncated};

    std::uint64_t value{0};
    (void)reader.read_varint(value);

    EXPECT_TRUE(reader.has_error());
    ASSERT_TRUE(reader.error().has_value());
    EXPECT_EQ(*reader.error(), core::status_code::corrupted);
}
