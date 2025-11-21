#include <algorithm>
#include <filesystem>
#include <iterator>
#include <random>
#include <array>

#include <fmt/core.h>

#include "common/uuid.h"

namespace common
{

template <typename TNumber>
auto generateRandomNumber(
    const TNumber min = std::numeric_limits<TNumber>::min(),
    const TNumber max = std::numeric_limits<TNumber>::max()
) noexcept -> TNumber
{
    std::mt19937 rng{std::random_device{}()};
    if constexpr (std::is_same_v<int, TNumber>)
    {
        return std::uniform_int_distribution<TNumber>(min, max)(rng);
    }
    else if (std::is_same_v<std::size_t, TNumber>)
    {
        return std::uniform_int_distribution<TNumber>(min, max)(rng);
    }
    else if (std::is_same_v<double, TNumber>)
    {
        return std::uniform_real_distribution<double>(min, max)(rng);
    }
    else if (std::is_same_v<float, TNumber>)
    {
        return std::uniform_real_distribution<float>(min, max)(rng);
    }
    else
    {
        // TODO(vahag): better handle this case
        return 0;
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
