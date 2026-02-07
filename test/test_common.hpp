#pragma once

#include <cstdint>
#include <random>
#include <string>

namespace frankie::testing {

std::string random_string_rng(std::mt19937_64& rng,
                              std::uint64_t length) noexcept;

std::string random_string(std::uint64_t length) noexcept;

std::uint64_t random_u64(std::uint64_t min, std::uint64_t max) noexcept;

}  // namespace frankie::testing
