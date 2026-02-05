#pragma once

#include <cstdint>
#include <string>

namespace frankie::testing {

std::uint64_t random_u64(std::uint64_t min, std::uint64_t max) noexcept;

std::string random_string(std::uint64_t length) noexcept;

}  // namespace frankie::testing
