#include "gtest/gtest.h"

#include "serialization/crc32.h"

using namespace serialization;

TEST(CRC32Test, TableGeneration)
{
    // First few known values from standard CRC32 table
    // These are from IEEE 802.3 / ZIP / PNG standards

    static constexpr const auto TABLE = detail::generate_crc32_table();
    EXPECT_EQ(TABLE[0], 0x00000000);
    EXPECT_EQ(TABLE[1], 0x77073096);
    EXPECT_EQ(TABLE[2], 0xEE0E612C);
    EXPECT_EQ(TABLE[255], 0x2D02EF8D);
}

TEST(CRC32Test, KnownValues)
{
    // Test against known CRC32 values

    // CRC32("") = 0x00000000
    crc32_t crc1;
    EXPECT_EQ(crc1.finalize(), 0x00000000);

    // CRC32("123456789") = 0xCBF43926
    crc32_t     crc2;
    std::string test = "123456789";
    crc2.update(std::as_bytes(std::span(test)));
    EXPECT_EQ(crc2.finalize(), 0xCBF43926);

    // CRC32("The quick brown fox jumps over the lazy dog") = 0x414FA339
    crc32_t     crc3;
    std::string test2 = "The quick brown fox jumps over the lazy dog";
    crc3.update(std::as_bytes(std::span(test2)));
    EXPECT_EQ(crc3.finalize(), 0x414FA339);
}

TEST(CRC32Test, IncrementalUpdate)
{
    // Verify incremental updates work
    std::string data = "Hello, World!";
    auto        substr1 = data.substr(0, 5);
    auto        substr2 = data.substr(5, 2);
    auto        substr3 = data.substr(7);

    // Single update
    crc32_t crc1;
    crc1.update(std::as_bytes(std::span(data)));
    uint32_t result1 = crc1.finalize();

    // Split into chunks
    crc32_t crc2;
    crc2.update(std::as_bytes(std::span(substr1))); // "Hello"
    crc2.update(std::as_bytes(std::span(substr2))); // ", "
    crc2.update(std::as_bytes(std::span(substr3))); // "World!"
    uint32_t result2 = crc2.finalize();

    EXPECT_EQ(result1, result2);
}
