#pragma once

#include <filesystem>
#include <random>
#include <vector>

#include <fmt/core.h>

namespace common
{

template <typename TNumber>
auto generateRandomNumber(
    const TNumber min = std::numeric_limits<TNumber>::min(),
    const TNumber max = std::numeric_limits<TNumber>::max()
) noexcept -> TNumber
{
    static thread_local std::mt19937 rng{std::random_device{}()};

    if constexpr (std::is_integral_v<TNumber>)
    {
        return std::uniform_int_distribution<TNumber>(min, max)(rng);
    }
    else if constexpr (std::is_floating_point_v<TNumber>)
    {
        return std::uniform_real_distribution<TNumber>(min, max)(rng);
    }
    else
    {
        static_assert(
            std::is_integral_v<TNumber> || std::is_floating_point_v<TNumber>,
            "TNumber must be an integral or floating-point type"
        );
    }
}

auto uuid() -> std::string;

auto segment_name() -> std::string;

auto segment_path(const std::filesystem::path &datadir, const std::string &name)
    -> std::filesystem::path;

auto generateRandomString(std::size_t length) noexcept -> std::string;

auto generateRandomStringPairVector(const std::size_t length) noexcept
    -> std::vector<std::pair<std::string, std::string>>;

} // namespace common
