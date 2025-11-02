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
