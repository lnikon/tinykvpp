#include <type_traits>

#include "gtest/gtest.h"

#include "serialization/endian_integer.h"

using namespace serialization;

template <typename T>
    requires std::is_integral_v<T>
auto to_bytes(T value) noexcept -> std::array<std::byte, sizeof(T)>
{
    std::array<std::byte, sizeof(T)> bytes;
    std::memcpy(bytes.data(), &value, sizeof(T));
    return bytes;
}

template <typename T, T V> struct TypeValuePair
{
    using Type = T;
    static constexpr const T Value = V;
};

using TestTypes = ::testing::Types<
    TypeValuePair<std::int8_t, 12>,
    TypeValuePair<std::uint8_t, 12>,
    TypeValuePair<std::int16_t, 1234>,
    TypeValuePair<std::uint16_t, 1234>,
    TypeValuePair<std::int32_t, -123456>,
    TypeValuePair<std::uint32_t, 123456>,
    TypeValuePair<std::int64_t, 123456789>,
    TypeValuePair<std::uint64_t, 123456789>>;

template <typename T> class EndianIntegerFixture : public testing::Test
{
  protected:
    using IntType = T::Type;
    static constexpr IntType Value = T::Value;
};

TYPED_TEST_SUITE(EndianIntegerFixture, TestTypes);

TYPED_TEST(EndianIntegerFixture, Get)
{
    using IntType = typename TestFixture::IntType;
    constexpr IntType podInt = TestFixture::Value;

    EXPECT_EQ(endian_integer<IntType>{podInt}.get(), podInt);
}

TYPED_TEST(EndianIntegerFixture, FromBytes)
{
    using IntType = typename TestFixture::IntType;
    constexpr IntType podInt = TestFixture::Value;

    EXPECT_TRUE(std::ranges::equal(endian_integer<IntType>{podInt}.bytes(), to_bytes(podInt)));
}
