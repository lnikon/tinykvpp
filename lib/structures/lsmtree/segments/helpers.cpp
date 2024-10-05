//
// Created by nikon on 3/8/24.
//

#include <iterator>
#include <structures/lsmtree/segments/helpers.h>

#include <random>
#include <array>
#include "uuid.h"

#include <fmt/core.h>

namespace structures::lsmtree::segments::helpers
{

auto uuid() -> std::string
{
    std::random_device rnd;
    auto               seed_data = std::array<int, std::mt19937::state_size>{};
    std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rnd));
    std::seed_seq                seq(std::begin(seed_data), std::end(seed_data));
    std::mt19937                 generator(seq);
    uuids::uuid_random_generator gen{generator};
    return to_string(uuids::basic_uuid_random_generator{gen}());
}

auto segment_name() -> types::name_t
{
    return types::name_t{fmt::format("segment_{}", uuid())};
}

auto segment_path(const std::filesystem::path &datadir, const types::name_t &name) -> std::filesystem::path
{
    return datadir / name;
}

} // namespace structures::lsmtree::segments::helpers
