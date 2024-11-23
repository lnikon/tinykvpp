#include "common.h"

namespace bench
{

auto generateRandomString(const std::size_t length) -> std::string
{
    constexpr const auto asciiRangeStart{32};
    constexpr const auto asciiRangeEnd{126};

    static std::random_device                 rng;
    static std::mt19937                       generator(rng());
    static std::uniform_int_distribution<int> distribution(asciiRangeStart, asciiRangeEnd);

    std::string result;
    result.resize(length);
    for (std::size_t i = 0; i < length; ++i)
    {
        result[i] = static_cast<char>(distribution(generator));
    }
    return result;
}

} // namespace bench
