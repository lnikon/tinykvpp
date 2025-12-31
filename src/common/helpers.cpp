#include <algorithm>
#include <iterator>
#include <array>

#include <spdlog/spdlog.h>

#include "common/uuid.h"
#include "common/helpers.h"

namespace common
{

auto uuid() -> std::string
{
    std::random_device rnd;
    auto               seed_data = std::array<int, std::mt19937::state_size>{};
    std::ranges::generate(seed_data, std::ref(rnd));

    std::seed_seq                seq(std::begin(seed_data), std::end(seed_data));
    std::mt19937                 generator(seq);
    uuids::uuid_random_generator gen{generator};

    return to_string(uuids::basic_uuid_random_generator{gen}());
}

auto segment_name() -> std::string
{
    return fmt::format("segment_{}", uuid());
}

auto segment_path(const std::filesystem::path &datadir, const std::string &name)
    -> std::filesystem::path
{
    return datadir / name;
}

auto generateRandomString(std::size_t length) noexcept -> std::string
{
    std::mt19937_64                         gen(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dis(0, 255);

    std::string result;
    result.reserve(length);

    size_t remaining = length;
    while (remaining >= sizeof(uint64_t))
    {
        uint64_t chunk = dis(gen);
        result.append(reinterpret_cast<char *>(&chunk), sizeof(uint64_t));
        remaining -= sizeof(uint64_t);
    }

    std::uniform_int_distribution<uint8_t> dis8;
    while ((remaining--) != 0U)
    {
        result += static_cast<char>(dis8(gen));
    }
    return result;
}

auto generateRandomStringPairVector(const std::size_t length) noexcept
    -> std::vector<std::pair<std::string, std::string>>
{
    std::vector<std::pair<std::string, std::string>> result;
    result.reserve(length);
    for (std::string::size_type size = 0; size < length; size++)
    {
        result.emplace_back(
            generateRandomString(generateRandomNumber<std::size_t>(64, 64)),
            generateRandomString(generateRandomNumber<std::size_t>(64, 64))
        );
    }
    return result;
}
} // namespace common
