//
// Created by nikon on 3/8/24.
//

#include <structures/lsmtree/segments/helpers.h>

#include <random>
#include <array>

#include <fmt/core.h>

namespace structures::lsmtree::segments::helpers
{

auto unix_timestamp()

{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string uuid()
{
    // Step 1: Generate 16 random bytes
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 255);

    std::array<uint8_t, 16> uuid;
    for (auto &byte : uuid)
    {
        byte = static_cast<uint8_t>(dis(gen));
    }

    // Step 2: Set the version (4) and variant (8, 9, A, or B)
    uuid[6] = (uuid[6] & 0x0F) | 0x40; // Set the version to 0100
    uuid[8] = (uuid[8] & 0x3F) | 0x80; // Set the variant to 10

    // Step 3: Convert to string
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < uuid.size(); ++i)
    {
        ss << std::setw(2) << static_cast<int>(uuid[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9)
        {
            ss << '-';
        }
    }

    return ss.str();
}

types::name_t segment_name()
{
    return types::name_t{fmt::format("segment_{}", uuid())};
}

std::filesystem::path segment_path(const std::filesystem::path datadir, const types::name_t &name)
{
    return datadir / name;
}

} // namespace structures::lsmtree::segments::helpers
